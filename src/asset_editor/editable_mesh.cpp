//
//  MeshZ - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"

static int GetOrAddEdge(EditableMesh& em, int v0, int v1)
{
    int fv0 = Min(v0, v1);
    int fv1 = Max(v0, v1);

    for (int i = 0; i < em.edge_count; i++)
    {
        EditableEdge& ee = em.edges[i];
        if (ee.v0 == fv0 && ee.v1 == fv1)
        {
            ee.triangle_count++;
            return i;
        }
    }

    // Not found - add it
    if (em.edge_count >= MAX_EDGES)
        return -1;

    int edge_index = em.edge_count++;
    EditableEdge& ee = em.edges[edge_index];
    ee.triangle_count = 1;
    ee.v0 = fv0;
    ee.v1 = fv1;
    return edge_index;
}

static Vec3 TriangleNormal(const Vec3& p0, const Vec3& p1, const Vec3& p2)
{
    Vec3 u = p1 - p0;
    Vec3 v = p2 - p0;
    Vec3 n = Cross(u, v);
    return Normalize(n);
}

static void UpdateNormals(EditableMesh& em)
{
    for (int i=0; i<em.triangle_count; i++)
    {
        const EditableVertex& v0 = em.vertices[em.triangles[i].v0];
        const EditableVertex& v1 = em.vertices[em.triangles[i].v1];
        const EditableVertex& v2 = em.vertices[em.triangles[i].v2];
        Vec3 p0 = Vec3{v0.position.x, v0.position.y, v0.height};
        Vec3 p1 = Vec3{v1.position.x, v1.position.y, v1.height};
        Vec3 p2 = Vec3{v2.position.x, v2.position.y, v2.height};
        em.triangles[i].normal = TriangleNormal(p0, p1, p2);
    }

    em.dirty = true;
}

static void UpdateEdges(EditableMesh& em)
{
    em.edge_count = 0;

    Vec2 min = em.vertices[0].position;
    Vec2 max = min;

    for (int i = 1; i < em.vertex_count; i++)
    {
        EditableVertex& ev = em.vertices[i];
        min = Min(ev.position, min);
        max = Max(ev.position, max);
    }

    em.bounds = {min, max};

    for (int i = 0; i < em.triangle_count; i++)
    {
        EditableTriangle& et = em.triangles[i];
        GetOrAddEdge(em, et.v0, et.v1);
        GetOrAddEdge(em, et.v1, et.v2);
        GetOrAddEdge(em, et.v2, et.v0);
    }
}

void MarkModified(EditableMesh& em)
{
    em.modified = true;
}

void MarkDirty(EditableMesh& em)
{
    em.dirty = true;
    UpdateEdges(em);
    UpdateNormals(em);
}

Mesh* ToMesh(EditableMesh* emesh)
{
    if (emesh->dirty)
    {
        Clear(emesh->builder);

        for (int i = 0; i < emesh->triangle_count; i++)
        {
            const EditableTriangle& tri = emesh->triangles[i];

            Vec2 uv_color = ColorUV(tri.color.x, tri.color.y);
            AddVertex(emesh->builder, emesh->vertices[tri.v0].position, tri.normal, uv_color, 0);
            AddVertex(emesh->builder, emesh->vertices[tri.v1].position, tri.normal, uv_color, 0);
            AddVertex(emesh->builder, emesh->vertices[tri.v2].position, tri.normal, uv_color, 0);
            AddTriangle(emesh->builder, i * 3, i * 3 + 1, i * 3 + 2);
        }

        Free(emesh->mesh);
        emesh->mesh = CreateMesh(ALLOCATOR_DEFAULT, emesh->builder, NAME_NONE);
    }

    return emesh->mesh;
}

void SetTriangleColor(EditableMesh* em, int index, const Vec2Int& color)
{
    if (index < 0 || index >= em->triangle_count)
        return;

    em->triangles[index].color = color;
    MarkModified(*em);
    MarkDirty(*em);
}

void SetSelectedTrianglesColor(EditableMesh& em, const Vec2Int& color)
{
    int count = 0;
    for (i32 i = 0; i < em.triangle_count; i++)
    {
        EditableTriangle& et = em.triangles[i];
        if (em.vertices[et.v0].selected && em.vertices[et.v1].selected && em.vertices[et.v2].selected)
        {
            et.color = color;
            count++;
        }
    }

    if (!count)
        return;

    MarkModified(em);
    MarkDirty(em);
}

void SetPosition(EditableMesh* em, int index, const Vec2& position)
{
    if (index < 0 || index >= em->vertex_count)
        return;

    em->vertices[index].position = position;
    MarkModified(*em);
    MarkDirty(*em);
}

void SetHeight(EditableMesh& em, int index, float height)
{
    assert(index >=0 && index < em.vertex_count);
    em.vertices[index].height = height;
    MarkModified(em);
    MarkDirty(em);
}

static void DissolveVertex(EditableMesh& em, int vertex_index)
{
    assert(vertex_index >= 0 && vertex_index < em.vertex_count);

    // Build ordered list of boundary edges around the vertex
    int boundary_edges[64][2]; // [edge_index][v0,v1]
    int boundary_count = 0;

    // Find all triangles using this vertex and collect their edges
    for (int i = 0; i < em.triangle_count; i++)
    {
        EditableTriangle& et = em.triangles[i];
        if (et.v0 == vertex_index || et.v1 == vertex_index || et.v2 == vertex_index)
        {
            // Get the edge opposite to the vertex being dissolved
            // This edge will become part of the boundary of the hole
            if (et.v0 == vertex_index)
            {
                // Opposite edge is v1->v2
                if (boundary_count < 64)
                {
                    boundary_edges[boundary_count][0] = et.v1;
                    boundary_edges[boundary_count][1] = et.v2;
                    boundary_count++;
                }
            }
            else if (et.v1 == vertex_index)
            {
                // Opposite edge is v2->v0
                if (boundary_count < 64)
                {
                    boundary_edges[boundary_count][0] = et.v2;
                    boundary_edges[boundary_count][1] = et.v0;
                    boundary_count++;
                }
            }
            else
            { // et.v2 == vertex_index
                // Opposite edge is v0->v1
                if (boundary_count < 64)
                {
                    boundary_edges[boundary_count][0] = et.v0;
                    boundary_edges[boundary_count][1] = et.v1;
                    boundary_count++;
                }
            }
        }
    }

    // Remove all triangles that reference this vertex
    for (int i = em.triangle_count - 1; i >= 0; i--)
    {
        EditableTriangle& et = em.triangles[i];
        if (et.v0 == vertex_index || et.v1 == vertex_index || et.v2 == vertex_index)
        {
            // Remove triangle by swapping with last and reducing count
            em.triangles[i] = em.triangles[em.triangle_count - 1];
            em.triangle_count--;
        }
    }

    // Remove duplicate edges (edges that appear twice are internal, not boundary)
    int filtered_edges[64][2];
    int filtered_count = 0;

    for (int i = 0; i < boundary_count; i++)
    {
        bool is_duplicate = false;
        for (int j = i + 1; j < boundary_count; j++)
        {
            // Check if edge i and j are the same (in either direction)
            if ((boundary_edges[i][0] == boundary_edges[j][0] && boundary_edges[i][1] == boundary_edges[j][1]) ||
                (boundary_edges[i][0] == boundary_edges[j][1] && boundary_edges[i][1] == boundary_edges[j][0]))
            {
                is_duplicate = true;
                break;
            }
        }

        if (!is_duplicate)
        {
            filtered_edges[filtered_count][0] = boundary_edges[i][0];
            filtered_edges[filtered_count][1] = boundary_edges[i][1];
            filtered_count++;
        }
    }

    // Order the boundary edges to form a continuous loop
    int ordered_vertices[64];
    int ordered_count = 0;

    if (filtered_count >= 2)
    {
        // Start with first edge
        ordered_vertices[ordered_count++] = filtered_edges[0][0];
        ordered_vertices[ordered_count++] = filtered_edges[0][1];
        bool used[64] = {true}; // Mark first edge as used
        for (int i = 1; i < filtered_count; i++)
            used[i] = false;

        // Find connecting edges
        while (ordered_count < filtered_count + 1)
        {
            int last_vertex = ordered_vertices[ordered_count - 1];
            bool found = false;

            for (int i = 1; i < filtered_count; i++)
            {
                if (used[i])
                    continue;

                if (filtered_edges[i][0] == last_vertex)
                {
                    ordered_vertices[ordered_count++] = filtered_edges[i][1];
                    used[i] = true;
                    found = true;
                    break;
                }
                else if (filtered_edges[i][1] == last_vertex)
                {
                    ordered_vertices[ordered_count++] = filtered_edges[i][0];
                    used[i] = true;
                    found = true;
                    break;
                }
            }

            if (!found)
                break;
        }

        // Remove duplicate last vertex if it connects back to first
        if (ordered_count > 2 && ordered_vertices[ordered_count - 1] == ordered_vertices[0])
            ordered_count--;

        // Triangulate the ordered polygon using fan triangulation
        for (int i = 1; i < ordered_count - 1; i++)
        {
            if (em.triangle_count < MAX_TRIANGLES)
            {
                EditableTriangle& new_tri = em.triangles[em.triangle_count++];
                new_tri.v0 = ordered_vertices[0];
                new_tri.v1 = ordered_vertices[i];
                new_tri.v2 = ordered_vertices[i + 1];
            }
        }
    }

    // Shift all vertices down as long as it wasn't the last vertex
    for (int i = vertex_index; i < em.vertex_count - 1; i++)
        em.vertices[i] = em.vertices[i + 1];

    em.vertex_count--;

    // Update all triangle vertex indices that are greater than vertex_index
    for (int i = 0; i < em.triangle_count; i++)
    {
        EditableTriangle& et = em.triangles[i];
        if (et.v0 > vertex_index)
            et.v0--;
        if (et.v1 > vertex_index)
            et.v1--;
        if (et.v2 > vertex_index)
            et.v2--;
    }

    MarkModified(em);
    MarkDirty(em);
}

void MergeSelectedVerticies(EditableMesh& em)
{
    // Find all selected vertices and calculate center
    Vec2 center = VEC2_ZERO;
    int selected_indices[MAX_VERTICES];
    int selected_count = 0;
    for (int i = 0; i < em.vertex_count; i++)
    {
        if (!em.vertices[i].selected)
            continue;

        center += em.vertices[i].position;
        selected_indices[selected_count++] = i;
    }

    if (selected_count <= 1)
        return;

    center = center * (1.0f / selected_count);

    int merged_vertex_index = selected_indices[0];
    em.vertices[merged_vertex_index].position = center;

    // Now dissolve all other vertices
    for (int i = selected_count - 1; i > 0; i--)
        DissolveVertex(em, selected_indices[i]);

    MarkModified(em);
    MarkDirty(em);
}

void DissolveSelectedVertices(EditableMesh& em)
{
    for (int i=em.vertex_count - 1; i>=0; i--)
    {
        EditableVertex& ev = em.vertices[i];
        if (!ev.selected)
            continue;

        DissolveVertex(em, i);
    }
}

void DeleteVertex(EditableMesh* em, int vertex_index)
{
    assert(em);
    assert(vertex_index >= 0 && vertex_index < em->vertex_count);

    // Remove any triangles that reference this vertex
    for (int i = em->triangle_count - 1; i >= 0; i--)
    {
        EditableTriangle& et = em->triangles[i];
        if (et.v0 == vertex_index || et.v1 == vertex_index || et.v2 == vertex_index)
        {
            // Remove triangle by swapping with last and reducing count
            em->triangles[i] = em->triangles[em->triangle_count - 1];
            em->triangle_count--;
        }
    }

    // Iterate over all triangles and decrement vertex indices greater than vertex_index
    for (int i = 0; i < em->triangle_count; i++)
    {
        EditableTriangle& et = em->triangles[i];
        if (et.v0 > vertex_index)
            et.v0--;
        if (et.v1 > vertex_index)
            et.v1--;
        if (et.v2 > vertex_index)
            et.v2--;
    }

    // shift all verticies down as long as it wasnt the last vertex
    for (int i = vertex_index; i < em->vertex_count - 1; i++)
        em->vertices[i] = em->vertices[i + 1];

    em->vertex_count--;

    // We could end up with vertices that no longer have a triangle, we need to find them and remove them
    for (int i = em->vertex_count - 1; i >= 0; i--)
    {
        bool used = false;
        for (int j = 0; j < em->triangle_count; j++)
        {
            EditableTriangle& et = em->triangles[j];
            if (et.v0 == i || et.v1 == i || et.v2 == i)
            {
                used = true;
                break;
            }
        }

        if (!used)
        {
            // Remove vertex by swapping with last and reducing count
            em->vertices[i] = em->vertices[em->vertex_count - 1];
            em->vertex_count--;

            // Iterate over all triangles and decrement vertex indices greater than i
            for (int j = 0; j < em->triangle_count; j++)
            {
                EditableTriangle& et = em->triangles[j];
                if (et.v0 > i)
                    et.v0--;
                if (et.v1 > i)
                    et.v1--;
                if (et.v2 > i)
                    et.v2--;
            }
        }
    }

    MarkModified(*em);
    MarkDirty(*em);
}

static int GetTriangleEdgeIndex(const EditableTriangle& et, const EditableEdge& ee)
{
    // Return 0, 1, or 2 if the edge matches one of the triangle's edges
    if ((et.v0 == ee.v0 && et.v1 == ee.v1) || (et.v0 == ee.v1 && et.v1 == ee.v0))
        return 0;

    if ((et.v1 == ee.v0 && et.v2 == ee.v1) || (et.v1 == ee.v1 && et.v2 == ee.v0))
        return 1;

    if ((et.v2 == ee.v0 && et.v0 == ee.v1) || (et.v2 == ee.v1 && et.v0 == ee.v0))
        return 2;

    return -1;
}

void RotateEdge(EditableMesh* em, int edge_index)
{
    assert(em);
    assert(edge_index >= 0 && edge_index < em->edge_count);

    EditableEdge& edge = em->edges[edge_index];

    // Find the two triangles that share this edge
    int triangle_indices[2];
    int triangle_count = 0;

    for (int i = 0; i < em->triangle_count; i++)
    {
        EditableTriangle& et = em->triangles[i];
        if (GetTriangleEdgeIndex(et, edge) != -1)
        {
            if (triangle_count < 2)
            {
                triangle_indices[triangle_count] = i;
                triangle_count++;
            }
        }
    }

    // Edge rotation only works if exactly 2 triangles share the edge
    if (triangle_count != 2)
        return;

    EditableTriangle& tri1 = em->triangles[triangle_indices[0]];
    EditableTriangle& tri2 = em->triangles[triangle_indices[1]];

    // Find the vertices that are NOT part of the shared edge
    int opposite1 = -1, opposite2 = -1;

    // Find opposite vertex in first triangle
    if (tri1.v0 != edge.v0 && tri1.v0 != edge.v1)
        opposite1 = tri1.v0;
    else if (tri1.v1 != edge.v0 && tri1.v1 != edge.v1)
        opposite1 = tri1.v1;
    else if (tri1.v2 != edge.v0 && tri1.v2 != edge.v1)
        opposite1 = tri1.v2;

    // Find opposite vertex in second triangle
    if (tri2.v0 != edge.v0 && tri2.v0 != edge.v1)
        opposite2 = tri2.v0;
    else if (tri2.v1 != edge.v0 && tri2.v1 != edge.v1)
        opposite2 = tri2.v1;
    else if (tri2.v2 != edge.v0 && tri2.v2 != edge.v1)
        opposite2 = tri2.v2;

    if (opposite1 == -1 || opposite2 == -1)
        return;

    // Create new triangles with the rotated edge, maintaining proper winding order
    // The new edge connects opposite1 and opposite2

    // We need to determine the correct winding order by checking the original triangles
    // Get positions to calculate winding
    Vec2 pos_opposite1 = em->vertices[opposite1].position;
    Vec2 pos_opposite2 = em->vertices[opposite2].position;
    Vec2 pos_v0 = em->vertices[edge.v0].position;
    Vec2 pos_v1 = em->vertices[edge.v1].position;

    // Calculate cross product to determine winding for first triangle
    // We want: opposite1 -> edge.v0 -> opposite2 to maintain CCW winding
    Vec2 edge1 = {pos_v0.x - pos_opposite1.x, pos_v0.y - pos_opposite1.y};
    Vec2 edge2 = {pos_opposite2.x - pos_opposite1.x, pos_opposite2.y - pos_opposite1.y};
    float cross1 = edge1.x * edge2.y - edge1.y * edge2.x;

    if (cross1 > 0)
    {
        // CCW winding
        tri1.v0 = opposite1;
        tri1.v1 = edge.v0;
        tri1.v2 = opposite2;
    }
    else
    {
        // CW winding - reverse order
        tri1.v0 = opposite1;
        tri1.v1 = opposite2;
        tri1.v2 = edge.v0;
    }

    // Calculate winding for second triangle
    // We want: opposite1 -> opposite2 -> edge.v1 or opposite1 -> edge.v1 -> opposite2
    Vec2 edge3 = {pos_opposite2.x - pos_opposite1.x, pos_opposite2.y - pos_opposite1.y};
    Vec2 edge4 = {pos_v1.x - pos_opposite1.x, pos_v1.y - pos_opposite1.y};
    float cross2 = edge3.x * edge4.y - edge3.y * edge4.x;

    if (cross2 > 0)
    {
        // CCW winding
        tri2.v0 = opposite1;
        tri2.v1 = opposite2;
        tri2.v2 = edge.v1;
    }
    else
    {
        // CW winding - reverse order
        tri2.v0 = opposite1;
        tri2.v1 = edge.v1;
        tri2.v2 = opposite2;
    }

    MarkModified(*em);
    MarkDirty(*em);
}

int SplitEdge(EditableMesh& em, int edge_index, float edge_pos)
{
    assert(edge_index >= 0 && edge_index < em.edge_count);

    if (em.vertex_count >= MAX_VERTICES)
        return -1;

    if (em.triangle_count + 2 >= MAX_TRIANGLES)
        return -1;

    EditableEdge& ee = em.edges[edge_index];
    EditableVertex& v0 = em.vertices[ee.v0];
    EditableVertex& v1 = em.vertices[ee.v1];

    int new_vertex_index = em.vertex_count++;
    EditableVertex& new_vertex = em.vertices[new_vertex_index];
    new_vertex.position = (v0.position * (1.0f - edge_pos) + v1.position * edge_pos);

    int triangle_count = em.triangle_count;
    for (int i = 0; i < triangle_count; i++)
    {
        EditableTriangle& et = em.triangles[i];

        int triangle_edge = GetTriangleEdgeIndex(et, ee);
        if (triangle_edge == -1)
            continue;

        EditableTriangle& split_tri = em.triangles[em.triangle_count++];
        split_tri.color = et.color;

        if (triangle_edge == 0)
        {
            split_tri.v0 = new_vertex_index;
            split_tri.v1 = et.v1;
            split_tri.v2 = et.v2;
            et.v1 = new_vertex_index;
        }
        else if (triangle_edge == 1)
        {
            split_tri.v0 = et.v0;
            split_tri.v1 = new_vertex_index;
            split_tri.v2 = et.v2;
            et.v2 = new_vertex_index;
        }
        else
        {
            split_tri.v0 = et.v0;
            split_tri.v1 = et.v1;
            split_tri.v2 = new_vertex_index;
            et.v0 = new_vertex_index;
        }
    }

    return new_vertex_index;
}

int HitTestVertex(const EditableMesh& em, const Vec2& world_pos, float size)
{
    for (int i = 0; i < em.vertex_count; i++)
    {
        const EditableVertex& ev = em.vertices[i];
        float dist = Length(world_pos - ev.position);
        if (dist < size)
            return i;
    }

    return -1;
}

int HitTestEdge(const EditableMesh& em, const Vec2& hit_pos, float size, float* where)
{
    for (int i = 0; i < em.edge_count; i++)
    {
        const EditableEdge& ee = em.edges[i];
        const Vec2& v0 = em.vertices[ee.v0].position;
        const Vec2& v1 = em.vertices[ee.v1].position;
        Vec2 edge_dir = Normalize(v1 - v0);
        Vec2 to_mouse = hit_pos - v0;
        float edge_length = Length(v1 - v0);
        float proj = Dot(to_mouse, edge_dir);
        if (proj >= 0 && proj <= edge_length)
        {
            Vec2 closest_point = v0 + edge_dir * proj;
            float dist = Length(hit_pos - closest_point);
            if (dist < size)
            {
                if (where)
                    *where = proj / edge_length;
                return i;
            }
        }
    }

    return -1;
}

bool HitTestTriangle(const EditableMesh& em, const EditableTriangle& et, const Vec2& position, const Vec2& hit_pos,
                     Vec2* where)
{
    Vec2 v0 = em.vertices[et.v0].position + position;
    Vec2 v1 = em.vertices[et.v1].position + position;
    Vec2 v2 = em.vertices[et.v2].position + position;

    // Calculate the area using cross product (can be negative if clockwise)
    float area = (v1.x - v0.x) * (v2.y - v0.y) - (v2.x - v0.x) * (v1.y - v0.y);

    // Handle degenerate triangles (zero area)
    if (fabsf(area) < 1e-6f)
        return false;

    // Calculate barycentric coordinates
    float inv_area = 1.0f / area;
    float s = ((v2.y - v0.y) * (hit_pos.x - v0.x) + (v0.x - v2.x) * (hit_pos.y - v0.y)) * inv_area;
    float t = ((v0.y - v1.y) * (hit_pos.x - v0.x) + (v1.x - v0.x) * (hit_pos.y - v0.y)) * inv_area;

    if (s >= 0 && t >= 0 && (s + t) <= 1)
    {
        if (where)
            *where = {s, t};

        return true;
    }

    return false;
}

int HitTestTriangle(const EditableMesh& mesh, const Vec2& position, const Vec2& hit_pos, Vec2* where)
{
    if (!Contains(mesh.bounds, hit_pos - position))
        return -1;

    for (int i = 0; i < mesh.triangle_count; i++)
    {
        const EditableTriangle& et = mesh.triangles[i];
        if (HitTestTriangle(mesh, et, position, hit_pos, where))
            return i;
    }

    return -1;
}

bool HitTest(const EditableMesh& mesh, const Vec2& position, const Bounds2& hit_bounds)
{
    return Intersects(mesh.bounds + position, hit_bounds);
}

Bounds2 GetSelectedBounds(const EditableMesh& emesh)
{
    Bounds2 bounds;
    bool first = true;
    for (int i = 0; i < emesh.vertex_count; i++)
    {
        const EditableVertex& ev = emesh.vertices[i];
        if (!ev.selected)
            continue;

        if (first)
            bounds = {ev.position, ev.position};
        else
            bounds = Union(bounds, ev.position);

        first = false;
    }

    return bounds;
}

EditableMesh* CreateEditableMesh(Allocator* allocator)
{
    EditableMesh* em = (EditableMesh*)Alloc(allocator, sizeof(EditableMesh));
    em->builder = CreateMeshBuilder(ALLOCATOR_DEFAULT, MAX_VERTICES, MAX_INDICES);
    em->vertex_count = 4;
    em->vertices[0] = {.position = {-0.5f, -0.5f}};
    em->vertices[1] = {.position = {0.5f, -0.5f}};
    em->vertices[2] = {.position = {0.5f, 0.5f}};
    em->vertices[3] = {.position = {-0.5f, 0.5f}};

    em->triangle_count = 2;
    em->triangles[0] = {0, 1, 2};
    em->triangles[1] = {0, 2, 3};

    MarkDirty(*em);

    return em;
}

void SetSelection(EditableMesh& em, int vertex_index)
{
    assert(vertex_index >= 0 && vertex_index < em.vertex_count);
    ClearSelection(em);
    AddSelection(em, vertex_index);
}

void ClearSelection(EditableMesh& em)
{
    for (int i=0; i<em.vertex_count; i++)
        em.vertices[i].selected = false;

    em.selected_vertex_count = 0;
}

void AddSelection(EditableMesh& em, int vertex_index)
{
    assert(vertex_index >= 0 && vertex_index < em.vertex_count);
    EditableVertex& ev = em.vertices[vertex_index];
    if (ev.selected)
        return;

    ev.selected = true;
    em.selected_vertex_count++;
}

void ToggleSelection(EditableMesh& em, int vertex_index)
{
    assert(vertex_index >= 0 && vertex_index < em.vertex_count);
    EditableVertex& ev = em.vertices[vertex_index];
    if (ev.selected)
    {
        ev.selected = false;
        em.selected_vertex_count--;
    }
    else
    {
        ev.selected = true;
        em.selected_vertex_count++;
    }
}