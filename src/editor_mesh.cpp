//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "editor_mesh.h"

#include "asset_editor/asset_editor.h"
#include "file_helpers.h"

static int GetOrAddEdge(EditorMesh& em, int v0, int v1)
{
    int fv0 = Min(v0, v1);
    int fv1 = Max(v0, v1);

    Vec2 en = Normalize(em.vertices[v1].position - em.vertices[v0].position);
    en = Vec2{en.y, -en.x};

    EditorVertex& ev0 = em.vertices[v0];
    EditorVertex& ev1 = em.vertices[v1];
    ev0.edge_normal += en;
    ev1.edge_normal += en;

    for (int i = 0; i < em.edge_count; i++)
    {
        EditorEdge& ee = em.edges[i];
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
    EditorEdge& ee = em.edges[edge_index];
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
    if (n.z < 0) n.z *= -1.0f;
    return Normalize(n);
}

static void UpdateNormals(EditorMesh& em)
{
    for (int i=0; i<em.face_count; i++)
    {
        const EditorVertex& v0 = em.vertices[em.faces[i].v0];
        const EditorVertex& v1 = em.vertices[em.faces[i].v1];
        const EditorVertex& v2 = em.vertices[em.faces[i].v2];
        Vec3 p0 = Vec3{v0.position.x, v0.position.y, v0.height};
        Vec3 p1 = Vec3{v1.position.x, v1.position.y, v1.height};
        Vec3 p2 = Vec3{v2.position.x, v2.position.y, v2.height};
        em.faces[i].normal = TriangleNormal(p0, p1, p2);
    }

    em.dirty = true;
}

static void UpdateEdges(EditorMesh& em)
{
    em.edge_count = 0;

    Vec2 min = em.vertices[0].position;
    Vec2 max = min;

    em.vertices[0].edge_normal = VEC2_ZERO;

    for (int i = 1; i < em.vertex_count; i++)
    {
        EditorVertex& ev = em.vertices[i];
        min = Min(ev.position, min);
        max = Max(ev.position, max);
        ev.edge_normal = VEC2_ZERO;
    }

    em.bounds = {min, max};

    for (int i = 0; i < em.face_count; i++)
    {
        EditorFace& et = em.faces[i];
        GetOrAddEdge(em, et.v0, et.v1);
        GetOrAddEdge(em, et.v1, et.v2);
        GetOrAddEdge(em, et.v2, et.v0);
    }

    for (int i = 1; i < em.vertex_count; i++)
    {
        EditorVertex& ev = em.vertices[i];
        //ev.edge_normal = Normalize(ev.edge_normal);
    }
}

void MarkModified(EditorMesh& em)
{
    em.modified = true;
}

void MarkDirty(EditorMesh& em)
{
    em.dirty = true;
    UpdateEdges(em);
    UpdateNormals(em);
}

Mesh* ToMesh(EditorMesh& em, bool upload)
{
    if (!em.dirty)
        return em.mesh;

    // Free old mesh
    Free(em.mesh);

    MeshBuilder* builder = CreateMeshBuilder(ALLOCATOR_DEFAULT, MAX_VERTICES, MAX_INDICES);

    // Generate the mesh body
    for (int i = 0; i < em.face_count; i++)
    {
        const EditorFace& tri = em.faces[i];
        Vec2 uv_color = ColorUV(tri.color.x, tri.color.y);
        AddVertex(builder, em.vertices[tri.v0].position, tri.normal, uv_color, 0);
        AddVertex(builder, em.vertices[tri.v1].position, tri.normal, uv_color, 0);
        AddVertex(builder, em.vertices[tri.v2].position, tri.normal, uv_color, 0);
        AddTriangle(builder, i * 3, i * 3 + 1, i * 3 + 2);
    }

    // Generate edges
    Vec2 edge_uv = ColorUV(0,0);
    for (int i=0; i < em.edge_count; i++)
    {
        constexpr float edge_width = 0.01f;

        const EditorEdge& ee = em.edges[i];
        if (ee.triangle_count > 1)
            continue;

        const EditorVertex& v0 = em.vertices[ee.v0];
        const EditorVertex& v1 = em.vertices[ee.v1];
        Vec3 p0 = Vec3{v0.position.x, v0.position.y, v0.height};
        Vec3 p1 = Vec3{v1.position.x, v1.position.y, v1.height};
        int base = GetVertexCount(builder);
        AddVertex(builder, ToVec2(p0), VEC3_FORWARD, edge_uv, 0);
        AddVertex(builder, ToVec2(p0) + v0.edge_normal * edge_width, VEC3_FORWARD, edge_uv, 0);
        AddVertex(builder, ToVec2(p1) + v1.edge_normal * edge_width, VEC3_FORWARD, edge_uv, 0);
        AddVertex(builder, ToVec2(p1), VEC3_FORWARD, edge_uv, 0);
        AddTriangle(builder, base+0, base+1, base+3);
        AddTriangle(builder, base+1, base+2, base+3);
    }

    em.mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE);
    Free(builder);

    return em.mesh;
}

void SetTriangleColor(EditorMesh* em, int index, const Vec2Int& color)
{
    if (index < 0 || index >= em->face_count)
        return;

    em->faces[index].color = color;
    MarkModified(*em);
    MarkDirty(*em);
}

void SetSelectedTrianglesColor(EditorMesh& em, const Vec2Int& color)
{
    int count = 0;
    for (i32 i = 0; i < em.face_count; i++)
    {
        EditorFace& et = em.faces[i];
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

void SetPosition(EditorMesh* em, int index, const Vec2& position)
{
    if (index < 0 || index >= em->vertex_count)
        return;

    em->vertices[index].position = position;
    MarkModified(*em);
    MarkDirty(*em);
}

void SetHeight(EditorMesh& em, int index, float height)
{
    assert(index >=0 && index < em.vertex_count);
    em.vertices[index].height = height;
    MarkModified(em);
    MarkDirty(em);
}

static void DissolveVertex(EditorMesh& em, int vertex_index)
{
    assert(vertex_index >= 0 && vertex_index < em.vertex_count);

    // Build ordered list of boundary edges around the vertex
    int boundary_edges[64][2]; // [edge_index][v0,v1]
    int boundary_count = 0;

    // Find all triangles using this vertex and collect their edges
    for (int i = 0; i < em.face_count; i++)
    {
        EditorFace& et = em.faces[i];
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
    for (int i = em.face_count - 1; i >= 0; i--)
    {
        EditorFace& et = em.faces[i];
        if (et.v0 == vertex_index || et.v1 == vertex_index || et.v2 == vertex_index)
        {
            // Remove triangle by swapping with last and reducing count
            em.faces[i] = em.faces[em.face_count - 1];
            em.face_count--;
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
            if (em.face_count < MAX_TRIANGLES)
            {
                EditorFace& new_tri = em.faces[em.face_count++];
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
    for (int i = 0; i < em.face_count; i++)
    {
        EditorFace& et = em.faces[i];
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

void MergeSelectedVerticies(EditorMesh& em)
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

void DissolveSelectedVertices(EditorMesh& em)
{
    for (int i=em.vertex_count - 1; i>=0; i--)
    {
        EditorVertex& ev = em.vertices[i];
        if (!ev.selected)
            continue;

        DissolveVertex(em, i);
    }
}

void DeleteVertex(EditorMesh* em, int vertex_index)
{
    assert(em);
    assert(vertex_index >= 0 && vertex_index < em->vertex_count);

    // Remove any triangles that reference this vertex
    for (int i = em->face_count - 1; i >= 0; i--)
    {
        EditorFace& et = em->faces[i];
        if (et.v0 == vertex_index || et.v1 == vertex_index || et.v2 == vertex_index)
        {
            // Remove triangle by swapping with last and reducing count
            em->faces[i] = em->faces[em->face_count - 1];
            em->face_count--;
        }
    }

    // Iterate over all triangles and decrement vertex indices greater than vertex_index
    for (int i = 0; i < em->face_count; i++)
    {
        EditorFace& et = em->faces[i];
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
        for (int j = 0; j < em->face_count; j++)
        {
            EditorFace& et = em->faces[j];
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
            for (int j = 0; j < em->face_count; j++)
            {
                EditorFace& et = em->faces[j];
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

static int GetTriangleEdgeIndex(const EditorFace& et, const EditorEdge& ee)
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

void RotateEdge(EditorMesh& em, int edge_index)
{
    assert(edge_index >= 0 && edge_index < em.edge_count);

    EditorEdge& edge = em.edges[edge_index];

    // Find the two triangles that share this edge
    int triangle_indices[2];
    int triangle_count = 0;

    for (int i = 0; i < em.face_count; i++)
    {
        EditorFace& et = em.faces[i];
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

    EditorFace& tri1 = em.faces[triangle_indices[0]];
    EditorFace& tri2 = em.faces[triangle_indices[1]];

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
    Vec2 pos_opposite1 = em.vertices[opposite1].position;
    Vec2 pos_opposite2 = em.vertices[opposite2].position;
    Vec2 pos_v0 = em.vertices[edge.v0].position;
    Vec2 pos_v1 = em.vertices[edge.v1].position;

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

    MarkModified(em);
    MarkDirty(em);
}

int SplitEdge(EditorMesh& em, int edge_index, float edge_pos)
{
    assert(edge_index >= 0 && edge_index < em.edge_count);

    if (em.vertex_count >= MAX_VERTICES)
        return -1;

    if (em.face_count + 2 >= MAX_TRIANGLES)
        return -1;

    EditorEdge& ee = em.edges[edge_index];
    EditorVertex& v0 = em.vertices[ee.v0];
    EditorVertex& v1 = em.vertices[ee.v1];

    int new_vertex_index = em.vertex_count++;
    EditorVertex& new_vertex = em.vertices[new_vertex_index];
    new_vertex.position = (v0.position * (1.0f - edge_pos) + v1.position * edge_pos);

    int triangle_count = em.face_count;
    for (int i = 0; i < triangle_count; i++)
    {
        EditorFace& et = em.faces[i];

        int triangle_edge = GetTriangleEdgeIndex(et, ee);
        if (triangle_edge == -1)
            continue;

        EditorFace& split_tri = em.faces[em.face_count++];
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

int SplitTriangle(EditorMesh& em, int triangle_index, const Vec2& position)
{
    assert(triangle_index >= 0 && triangle_index < em.face_count);

    if (em.vertex_count >= MAX_VERTICES)
        return -1;

    if (em.face_count + 2 >= MAX_TRIANGLES)
        return -1;

    EditorFace& et = em.faces[triangle_index];

    // Create new vertex at the position
    int new_vertex_index = em.vertex_count++;
    EditorVertex& new_vertex = em.vertices[new_vertex_index];
    new_vertex.position = position;
    new_vertex.height = 0.0f;
    new_vertex.saved_height = 0.0f;
    new_vertex.selected = false;

    // Create two new triangles
    EditorFace& tri1 = em.faces[em.face_count++];
    EditorFace& tri2 = em.faces[em.face_count++];

    // Copy color from original triangle
    tri1.color = et.color;
    tri2.color = et.color;

    // Split original triangle into three triangles:
    // Original: (v0, v1, new_vertex)
    // tri1: (v1, v2, new_vertex)
    // tri2: (v2, v0, new_vertex)

    // Modify original triangle
    int original_v2 = et.v2;
    et.v2 = new_vertex_index;

    // First new triangle
    tri1.v0 = et.v1;
    tri1.v1 = original_v2;
    tri1.v2 = new_vertex_index;

    // Second new triangle
    tri2.v0 = original_v2;
    tri2.v1 = et.v0;
    tri2.v2 = new_vertex_index;

    return new_vertex_index;
}

int HitTestVertex(const EditorMesh& em, const Vec2& world_pos, float size)
{
    for (int i = 0; i < em.vertex_count; i++)
    {
        const EditorVertex& ev = em.vertices[i];
        float dist = Length(world_pos - ev.position);
        if (dist < size)
            return i;
    }

    return -1;
}

int HitTestEdge(const EditorMesh& em, const Vec2& hit_pos, float size, float* where)
{
    for (int i = 0; i < em.edge_count; i++)
    {
        const EditorEdge& ee = em.edges[i];
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

bool HitTestTriangle(const EditorMesh& em, const EditorFace& et, const Vec2& position, const Vec2& hit_pos,
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

int HitTestTriangle(const EditorMesh& mesh, const Vec2& position, const Vec2& hit_pos, Vec2* where)
{
    if (!Contains(mesh.bounds, hit_pos - position))
        return -1;

    for (int i = 0; i < mesh.face_count; i++)
    {
        const EditorFace& et = mesh.faces[i];
        if (HitTestTriangle(mesh, et, position, hit_pos, where))
            return i;
    }

    return -1;
}

bool HitTest(const EditorMesh& mesh, const Vec2& position, const Bounds2& hit_bounds)
{
    return Intersects(mesh.bounds + position, hit_bounds);
}

Bounds2 GetSelectedBounds(const EditorMesh& emesh)
{
    Bounds2 bounds;
    bool first = true;
    for (int i = 0; i < emesh.vertex_count; i++)
    {
        const EditorVertex& ev = emesh.vertices[i];
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

EditorMesh* CreateEditableMesh(Allocator* allocator)
{
    EditorMesh* em = (EditorMesh*)Alloc(allocator, sizeof(EditorMesh));
    MarkDirty(*em);
    return em;
}

void SetSelection(EditorMesh& em, int vertex_index)
{
    assert(vertex_index >= 0 && vertex_index < em.vertex_count);
    ClearSelection(em);
    AddSelection(em, vertex_index);
}

void ClearSelection(EditorMesh& em)
{
    for (int i=0; i<em.vertex_count; i++)
        em.vertices[i].selected = false;

    em.selected_vertex_count = 0;
}

void AddSelection(EditorMesh& em, int vertex_index)
{
    assert(vertex_index >= 0 && vertex_index < em.vertex_count);
    EditorVertex& ev = em.vertices[vertex_index];
    if (ev.selected)
        return;

    ev.selected = true;
    em.selected_vertex_count++;
}

void ToggleSelection(EditorMesh& em, int vertex_index)
{
    assert(vertex_index >= 0 && vertex_index < em.vertex_count);
    EditorVertex& ev = em.vertices[vertex_index];
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

void SelectAll(EditorMesh& em)
{
    for (int i=0; i<em.vertex_count; i++)
        em.vertices[i].selected = true;

    em.selected_vertex_count = em.vertex_count;
}

int AddVertex(EditorMesh& em, const Vec2& position)
{
    constexpr float VERTEX_HIT_SIZE = 0.08f * 5.0f;

    // If on a vertex then return -1
    int vertex_index = HitTestVertex(em, position, VERTEX_HIT_SIZE);
    if (vertex_index != -1)
        return -1;

    // If on an edge then split the edge and add the point
    float edge_pos;
    int edge_index = HitTestEdge(em, position, VERTEX_HIT_SIZE, &edge_pos);
    if (edge_index >= 0)
    {
        int new_vertex = SplitEdge(em, edge_index, edge_pos);
        if (new_vertex != -1)
        {
            MarkDirty(em);
            MarkModified(em);
        }
        return new_vertex;
    }

    // If inside a triangle then split the triangle into three triangles and add the point
    Vec2 tri_pos;
    int triangle_index = HitTestTriangle(em, VEC2_ZERO, position, &tri_pos);
    if (triangle_index >= 0)
    {
        int new_vertex = SplitTriangle(em, triangle_index, position);
        if (new_vertex != -1)
        {
            MarkDirty(em);
            MarkModified(em);
        }
        return new_vertex;
    }

    // If outside all triangles, find the closest edge and create a triangle with it
    int closest_edge = -1;
    float closest_dist = FLT_MAX;
    Vec2 closest_point;

    for (int i = 0; i < em.edge_count; i++)
    {
        const EditorEdge& ee = em.edges[i];
        const Vec2& v0 = em.vertices[ee.v0].position;
        const Vec2& v1 = em.vertices[ee.v1].position;

        // Calculate closest point on edge to position
        Vec2 edge_dir = v1 - v0;
        float edge_length_sq = Dot(edge_dir, edge_dir);
        if (edge_length_sq < 1e-6f) continue; // Skip degenerate edges

        Vec2 to_pos = position - v0;
        float t = Clamp(Dot(to_pos, edge_dir) / edge_length_sq, 0.0f, 1.0f);
        Vec2 point_on_edge = v0 + edge_dir * t;
        float dist = Length(position - point_on_edge);

        if (dist < closest_dist)
        {
            closest_dist = dist;
            closest_edge = i;
            closest_point = point_on_edge;
        }
    }

    // If no edge found or mesh is empty, create a standalone vertex
    if (closest_edge == -1 || em.vertex_count >= MAX_VERTICES)
    {
        if (em.vertex_count >= MAX_VERTICES)
            return -1;

        // Create a standalone vertex
        int new_vertex_index = em.vertex_count++;
        EditorVertex& new_vertex = em.vertices[new_vertex_index];
        new_vertex.position = position;
        new_vertex.height = 0.0f;
        new_vertex.saved_height = 0.0f;
        new_vertex.selected = false;

        MarkDirty(em);
        MarkModified(em);
        return new_vertex_index;
    }

    // Create triangle with closest edge if we have room
    if (em.face_count >= MAX_TRIANGLES)
        return -1;

    if (em.vertex_count >= MAX_VERTICES)
        return -1;

    // Create new vertex
    int new_vertex_index = em.vertex_count++;
    EditorVertex& new_vertex = em.vertices[new_vertex_index];
    new_vertex.position = position;
    new_vertex.height = 0.0f;
    new_vertex.saved_height = 0.0f;
    new_vertex.selected = false;

    // Create triangle with the closest edge
    const EditorEdge& ee = em.edges[closest_edge];
    EditorFace& new_triangle = em.faces[em.face_count++];
    new_triangle.v0 = ee.v0;
    new_triangle.v1 = ee.v1;
    new_triangle.v2 = new_vertex_index;
    new_triangle.color = {0, 0}; // Default color

    MarkDirty(em);
    MarkModified(em);
    return new_vertex_index;
}

void FixNormals(EditorMesh& em)
{
    for (int i=0; i<em.face_count; i++)
    {
        // Ensure all triangles have CCW winding
        EditorFace& et = em.faces[i];
        const Vec2& v0 = em.vertices[et.v0].position;
        const Vec2& v1 = em.vertices[et.v1].position;
        const Vec2& v2 = em.vertices[et.v2].position;

        Vec2 e0 = v1 - v0;
        Vec2 e1 = v2 - v0;
        float cross = e0.x * e1.y - e0.y * e1.x;
        if (cross < 0)
        {
            // Swap v1 and v2 to change winding
            int temp = et.v1;
            et.v1 = et.v2;
            et.v2 = temp;
        }
    }
}

EditorMesh* Clone(Allocator* allocator, const EditorMesh& em)
{
    EditorMesh* clone = CreateEditableMesh(allocator);
    *clone = em;
    clone->mesh = nullptr;
    return clone;
}


void Copy(EditorMesh& dst, const EditorMesh& src)
{
    if (dst.mesh)
        Free(dst.mesh);

    dst = src;
    dst.mesh = nullptr;
    dst.dirty = true;
    dst.modified = true;
}

static void Optimize(EditorMesh& em)
{
    int* vertex_map0 = (int*)Alloc(ALLOCATOR_DEFAULT, sizeof(int) * em.vertex_count);
    int* vertex_map1 = (int*)Alloc(ALLOCATOR_DEFAULT, sizeof(int) * em.vertex_count);

    for (int i=0; i<em.vertex_count; i++)
    {
        if (vertex_map0[i] != 0)
            continue;

        EditorVertex& evi = em.vertices[i];
        vertex_map0[i] = i + 1;

        for (int j=em.vertex_count-1; j>i; j--)
        {
            EditorVertex& evj = em.vertices[j];
            if (vertex_map0[j] != 0)
                continue;

            if (ApproxEqual(evi.position, evj.position))
                vertex_map0[j] = i + 1;
        }
    }

    // Build the final vertex list from the vertex map0
    int vertex_count = 0;
    for (int i=0; i<em.vertex_count; i++)
    {
        if (vertex_map0[i] != i + 1)
        {
            vertex_map1[i] = vertex_map1[vertex_map0[i]-1];
            continue;
        }

        em.vertices[vertex_count++] = em.vertices[i];
        vertex_map1[i] = vertex_count;
    }

    for (int i=0; i<em.face_count; i++)
    {
        EditorFace& ef = em.faces[i];
        if (vertex_map1[ef.v0] != 0)
            ef.v0 = vertex_map1[ef.v0] - 1;
        if (vertex_map1[ef.v1] != 0)
            ef.v1 = vertex_map1[ef.v1] - 1;
        if (vertex_map1[ef.v2] != 0)
            ef.v2 = vertex_map1[ef.v2] - 1;
    }

    em.vertex_count = vertex_count;
}

EditorMesh* LoadEditorMesh(Allocator* allocator, const std::filesystem::path& path)
{
    std::string contents = ReadAllText(ALLOCATOR_DEFAULT, path);
    Tokenizer tk;
    Init(tk, contents.c_str());
    Token token={};

    EditorMesh* em = CreateEditableMesh(allocator);

    while (HasTokens(tk))
    {
        if (PeekChar(tk) == '\n')
        {
            NextChar(tk);
            SkipWhitespace(tk);
            continue;
        }

        if (!ExpectIdentifier(tk, &token))
            break;

        if (token.length != 1)
            break;

        if (token.value[0] == 'v')
        {
            f32 x;
            f32 y;
            if (!ExpectFloat(tk, &token, &x) || !ExpectFloat(tk, &token, &y))
                break;

            EditorVertex& ev = em->vertices[em->vertex_count++];
            ev.position = {x, y};
            ev.edge_size = 1.0f;

            SkipWhitespace(tk);

            while (PeekChar(tk) == 'e' || PeekChar(tk) == 'h')
            {
                char c = NextChar(tk);
                SkipWhitespace(tk);
                switch (c)
                {
                case 'e':
                {
                    float e;
                    if (!ExpectFloat(tk, &token, &e))
                        break;

                    ev.edge_size = e;

                    SkipWhitespace(tk);

                    break;
                }

                case 'h':
                {
                    float h;
                    if (!ExpectFloat(tk, &token, &h))
                        break;

                    ev.height = h;

                    SkipWhitespace(tk);

                    break;
                }

                default:
                    break;
                }
            }
        }
        else if (token.value[0] == 'f')
        {
            int v0;
            int v1;
            int v2;
            if (!ExpectInt(tk, &token, &v0) || !ExpectInt(tk, &token, &v1) || !ExpectInt(tk, &token, &v2))
                break;

            EditorFace& ef = em->faces[em->face_count++];
            ef.v0 = v0;
            ef.v1 = v1;
            ef.v2 = v2;

            SkipWhitespace(tk);

            while (PeekChar(tk) == 'c')
            {
                char c = NextChar(tk);
                SkipWhitespace(tk);
                switch (c)
                {
                case 'c':
                {
                    int cx;
                    int cy;
                    if (!ExpectInt(tk, &token, &cx) || !ExpectInt(tk, &token, &cy))
                        break;

                    ef.color = { (u8)cx, (u8)cy };

                    SkipWhitespace(tk);

                    break;
                }

                default:
                    break;
                }
            }
        }
    }

    Bounds2 bounds = { em->vertices[0].position, em->vertices[0].position };
    for (int i=0; i<em->vertex_count; i++)
    {
        bounds.min = Min(bounds.min, em->vertices[i].position);
        bounds.max = Max(bounds.max, em->vertices[i].position);
    }

    em->bounds = bounds;

    MarkDirty(*em);

    return em;
}

void SaveEditorMesh(const EditorMesh& em, const std::filesystem::path& path)
{
    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);

    for (int i=0; i<em.vertex_count; i++)
    {
        const EditorVertex& ev = em.vertices[i];
        WriteCSTR(stream, "v %g %g e %g h %g\n", ev.position.x, ev.position.y, ev.edge_size, ev.height);
    }

    WriteCSTR(stream, "\n");

    for (int i=0; i<em.face_count; i++)
    {
        const EditorFace& ef = em.faces[i];
        WriteCSTR(stream, "f %d %d %d c %d %d\n", ef.v0, ef.v1, ef.v2, ef.color.x, ef.color.y);
    }

    SaveStream(stream, path);
    Free(stream);
}

EditorAsset* CreateNewEditorMesh(const std::filesystem::path& path)
{
    const char* default_mesh = "v -1 -1 e 1 h 0\n"
                               "v 1 -1 e 1 h 0\n"
                               "v 1 1 e 1 h 0\n"
                               "v -1 1 e 1 h 0\n"
                               "\n"
                               "f 0 1 2 c 1 0\n"
                               "f 0 2 3 c 1 0\n";

    std::filesystem::path full_path = path.is_relative() ?  std::filesystem::current_path() / "assets" / path : path;
    full_path += ".mesh";

    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);
    WriteCSTR(stream, default_mesh);
    SaveStream(stream, full_path);
    Free(stream);

    EditorAsset* ea = CreateEditableAsset(full_path, LoadEditorMesh(ALLOCATOR_DEFAULT, full_path));
    if (!ea)
        return nullptr;

    g_asset_editor.assets[g_asset_editor.asset_count++] = ea;
    return ea;
}
