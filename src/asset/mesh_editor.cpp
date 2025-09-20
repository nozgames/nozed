//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

constexpr float OUTLINE_WIDTH = 0.05f;

#include "mesh_editor.h"

#include <editor.h>
#include <utils/file_helpers.h>

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

static void EditorMeshDraw(EditorAsset& ea)
{
    if (g_view.draw_mode == VIEW_DRAW_MODE_WIREFRAME)
    {
        BindColor(COLOR_EDGE);
        DrawEdges(ea.mesh, ea.position);
    }
    else
    {
        BindColor(COLOR_WHITE);
        DrawMesh(ea.mesh, Translate(ea.position));
    }
}

void DrawMesh(EditorMesh& em, const Mat3& transform)
{
    if (g_view.draw_mode == VIEW_DRAW_MODE_WIREFRAME)
        return;

    BindMaterial(g_view.draw_mode == VIEW_DRAW_MODE_SHADED ? g_view.shaded_material : g_view.solid_material);
    DrawMesh(ToMesh(em), transform);
}

Vec2 GetFaceCenter(EditorMesh& em, int face_index)
{
    const EditorFace& ef = em.faces[face_index];
    const EditorVertex& v0 = em.vertices[ef.v0];
    const EditorVertex& v1 = em.vertices[ef.v1];
    const EditorVertex& v2 = em.vertices[ef.v2];
    return (v0.position + v1.position + v2.position) / 3.0f;
}

bool IsVertexOnOutsideEdge(EditorMesh& em, int v0)
{
    for (int i = 0; i < em.edge_count; i++)
    {
        EditorEdge& ee = em.edges[i];
        if (ee.triangle_count == 1 && (ee.v0 == v0 || ee.v1 == v0))
            return true;
    }

    return false;
}

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

// static Vec3 TriangleNormal(const Vec3& p0, const Vec3& p1, const Vec3& p2)
// {
//     Vec3 u = p1 - p0;
//     Vec3 v = p2 - p0;
//     Vec3 n = Cross(u, v);
//     if (n.z < 0) n.z *= -1.0f;
//     return Normalize(n);
// }

static int TriangleWinding(const Vec2& p0, const Vec2& p1, const Vec2& p2)
{
    float area = (p1.x - p0.x) * (p2.y - p0.y) - (p2.x - p0.x) * (p1.y - p0.y);
    if (area > 0)
        return 1; // Counter-clockwise

    if (area < 0)
        return -1; // Clockwise

    return 0; // Degenerate
}

static bool FixWinding(const EditorMesh& em, EditorFace& ef)
{
    if (TriangleWinding(em.vertices[ef.v0].position, em.vertices[ef.v1].position, em.vertices[ef.v2].position) >= 0)
        return false;

    // Swap v1 and v2 to fix winding
    int temp = ef.v1;
    ef.v1 = ef.v2;
    ef.v2 = temp;
    return true;
}

// static void UpdateNormals(EditorMesh& em)
// {
//     for (int i=0; i<em.face_count; i++)
//     {
//         const EditorVertex& v0 = em.vertices[em.faces[i].v0];
//         const EditorVertex& v1 = em.vertices[em.faces[i].v1];
//         const EditorVertex& v2 = em.vertices[em.faces[i].v2];
//         Vec3 p0 = Vec3{v0.position.x, v0.position.y, v0.height};
//         Vec3 p1 = Vec3{v1.position.x, v1.position.y, v1.height};
//         Vec3 p2 = Vec3{v2.position.x, v2.position.y, v2.height};
//         em.faces[i].normal = TriangleNormal(p0, p1, p2);
//     }
// }

void UpdateEdges(EditorMesh& em)
{
    em.edge_count = 0;

    for (int i = 0; i < em.face_count; i++)
    {
        EditorFace& et = em.faces[i];
        GetOrAddEdge(em, et.v0, et.v1);
        GetOrAddEdge(em, et.v1, et.v2);
        GetOrAddEdge(em, et.v2, et.v0);
    }
}

void MarkDirty(EditorMesh& em)
{
    Free(em.mesh);
    em.mesh = nullptr;
}

Mesh* ToMesh(EditorMesh& em, bool upload)
{
    if (em.mesh)
        return em.mesh;

    MeshBuilder* builder = CreateMeshBuilder(ALLOCATOR_DEFAULT, MAX_VERTICES, MAX_INDICES);

    // Generate the mesh body
    for (int i = 0; i < em.face_count; i++)
    {
        const EditorFace& tri = em.faces[i];
        Vec2 uv_color = ColorUV(tri.color.x, tri.color.y);
        AddVertex(builder, em.vertices[tri.v0].position, tri.normal, uv_color);
        AddVertex(builder, em.vertices[tri.v1].position, tri.normal, uv_color);
        AddVertex(builder, em.vertices[tri.v2].position, tri.normal, uv_color);
        AddTriangle(builder, (u16)(i * 3), (u16)(i * 3 + 1), (u16)(i * 3 + 2));
    }

    // Generate outline
    Vec2 edge_uv = ColorUV(em.edge_color.x, em.edge_color.y);
    for (int i=0; i < em.edge_count; i++)
    {
        const EditorEdge& ee = em.edges[i];
        if (ee.triangle_count > 1)
            continue;

        const EditorVertex& v0 = em.vertices[ee.v0];
        const EditorVertex& v1 = em.vertices[ee.v1];

        if (v0.edge_size < 0.01f && v1.edge_size < 0.01f)
            continue;

        Vec3 en = {v0.edge_normal.x, v0.edge_normal.y, 0};
        for (int face_index=0; face_index < em.face_count; face_index++)
        {
            EditorFace& ef = em.faces[face_index];
            if (-1 != GetTriangleEdgeIndex(ef, ee))
            {
                // This edge is part of a face - use the face normal
                en = Vec3{ef.normal.x, ef.normal.y, 0.1f};
                break;
            }
        }

        Vec3 p0 = Vec3{v0.position.x, v0.position.y, v0.height};
        Vec3 p1 = Vec3{v1.position.x, v1.position.y, v1.height};
        u16 base = GetVertexCount(builder);
        AddVertex(builder, ToVec2(p0), en, edge_uv);
        AddVertex(builder, ToVec2(p0) + v0.edge_normal * v0.edge_size * OUTLINE_WIDTH, en, edge_uv);
        AddVertex(builder, ToVec2(p1) + v1.edge_normal * v1.edge_size * OUTLINE_WIDTH, en, edge_uv);
        AddVertex(builder, ToVec2(p1), en, edge_uv);
        AddTriangle(builder, base+0, base+1, base+3);
        AddTriangle(builder, base+1, base+2, base+3);
    }

    em.mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE, upload);
    Free(builder);

    em.bounds = GetBounds(em.mesh);

    return em.mesh;
}

void SetEdgeColor(EditorMesh& em, const Vec2Int& color)
{
    em.edge_color = color;
    MarkDirty(em);
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

    MarkDirty(em);
}

static void UpdateRefCounts(EditorMesh& em)
{
    for (int i=0; i<em.vertex_count; i++)
        em.vertices[i].ref_count = 0;

    for (int i=0; i<em.face_count; i++)
    {
        EditorFace& ef = em.faces[i];
        em.vertices[ef.v0].ref_count++;
        em.vertices[ef.v1].ref_count++;
        em.vertices[ef.v2].ref_count++;
    }
}

static void DeleteVertex(EditorMesh& em, int vertex_index)
{
    assert(vertex_index >= 0 && vertex_index < em.vertex_count);

    for (int edge_index=em.edge_count-1; edge_index >= 0; edge_index--)
    {
        EditorEdge& ee = em.edges[edge_index];
        if (ee.v0 == vertex_index || ee.v1 == vertex_index)
        {
            em.edges[edge_index] = em.edges[--em.edge_count];
            continue;
        }
        if (ee.v0 > vertex_index) ee.v0--;
        if (ee.v1 > vertex_index) ee.v1--;
    }

    for (int face_index=em.face_count-1; face_index >= 0; face_index--)
    {
        EditorFace& ef = em.faces[face_index];
        if (ef.v0 == vertex_index || ef.v1 == vertex_index || ef.v2 == vertex_index)
        {
            em.faces[face_index] = em.faces[--em.face_count];
            continue;
        }

        if (ef.v0 > vertex_index) ef.v0--;
        if (ef.v1 > vertex_index) ef.v1--;
        if (ef.v2 > vertex_index) ef.v2--;
    }

    for (int i=vertex_index; i<em.vertex_count-1; i++)
        em.vertices[i] = em.vertices[i+1];

    em.vertex_count--;

    UpdateRefCounts(em);

    for (int i=em.vertex_count-1; i>=0; i--)
        if (em.vertices[i].ref_count == 0)
        {
            DeleteVertex(em, i);
            return;
        }

    UpdateEdges(em);
    MarkDirty(em);
}

static void DeleteFace(EditorMesh& em, int face_index)
{
    assert(face_index >= 0 && face_index < em.face_count);

    em.faces[face_index] = em.faces[em.face_count - 1];
    em.face_count--;

    MarkDirty(em);
}

static void DissolveFace(EditorMesh& em, int face_index)
{
    assert(face_index >= 0 && face_index < em.face_count);
    char vertex_count[MAX_VERTICES] = {};

    for (int i=0; i<em.face_count; i++)
    {
        if (i == face_index)
            continue;

        EditorFace& ef = em.faces[i];
        vertex_count[ef.v0] = 1;
        vertex_count[ef.v1] = 1;
        vertex_count[ef.v2] = 1;
    }

    EditorFace& df = em.faces[face_index];
    if (vertex_count[df.v0] && vertex_count[df.v1] && vertex_count[df.v2])
    {
        DeleteFace(em, face_index);
        return;
    }

    for (int vertex_index=em.vertex_count-1; vertex_index>=0; vertex_index--)
    {
        if (vertex_count[vertex_index])
            continue;

        DeleteVertex(em, vertex_index);
    }
}

void DissolveSelectedFaces(EditorMesh& em)
{
    for (int face_index=em.face_count - 1; face_index>=0; face_index--)
    {
        EditorFace& ef = em.faces[face_index];
        if (!ef.selected)
            continue;

        DissolveFace(em, face_index);
    }
}

void DissolveSelectedVertices(EditorMesh& em)
{
    for (int vertex_index=em.vertex_count - 1; vertex_index>=0; vertex_index--)
    {
        EditorVertex& ev = em.vertices[vertex_index];
        if (!ev.selected)
            continue;

        DeleteVertex(em, vertex_index);
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

    MarkDirty(*em);
}

static float CalculateTriangleArea(const Vec2& v0, const Vec2& v1, const Vec2& v2)
{
    // Calculate signed area using cross product
    return (v1.x - v0.x) * (v2.y - v0.y) - (v2.x - v0.x) * (v1.y - v0.y);
}



int RotateEdge(EditorMesh& em, int edge_index)
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
        return -1;

    EditorFace& f1 = em.faces[triangle_indices[0]];
    EditorFace& f2 = em.faces[triangle_indices[1]];

    int opposite1 = -1;
    int opposite2 = -1;

    // Find opposite vertex in first triangle
    if (f1.v0 != edge.v0 && f1.v0 != edge.v1)
        opposite1 = f1.v0;
    else if (f1.v1 != edge.v0 && f1.v1 != edge.v1)
        opposite1 = f1.v1;
    else if (f1.v2 != edge.v0 && f1.v2 != edge.v1)
        opposite1 = f1.v2;

    // Find opposite vertex in second triangle
    if (f2.v0 != edge.v0 && f2.v0 != edge.v1)
        opposite2 = f2.v0;
    else if (f2.v1 != edge.v0 && f2.v1 != edge.v1)
        opposite2 = f2.v1;
    else if (f2.v2 != edge.v0 && f2.v2 != edge.v1)
        opposite2 = f2.v2;

    if (opposite1 == -1 || opposite2 == -1)
        return - 1;

    EditorFace f1n { opposite1, opposite2, edge.v0 };
    EditorFace f2n { opposite1, edge.v1, opposite2 };
    bool f1w = FixWinding(em, f1n);
    bool f2w = FixWinding(em, f2n);
    if (f1w != f2w)
        return -1;

    float f1a = fabsf(CalculateTriangleArea(em.vertices[f1n.v0].position, em.vertices[f1n.v1].position, em.vertices[f1n.v2].position));
    float f2a = fabsf(CalculateTriangleArea(em.vertices[f2n.v0].position, em.vertices[f2n.v1].position, em.vertices[f2n.v2].position));
    float a = f1a + f2a;
    if (a <= 1e-6f)
        return -1;

    constexpr float MIN_AREA_RATIO = 0.05f;
    if (Min(f1a,f2a) / a < MIN_AREA_RATIO)
        return -1;

    f1.v0 = f1n.v0;
    f1.v1 = f1n.v1;
    f1.v2 = f1n.v2;
    f2.v0 = f2n.v0;
    f2.v1 = f2n.v1;
    f2.v2 = f2n.v2;

    MarkDirty(em);

    // find the new edge index
    for (int i=0; i<em.edge_count; i++)
    {
        const EditorEdge& ee = em.edges[i];
        if ((ee.v0 == opposite1 && ee.v1 == opposite2) || (ee.v0 == opposite2 && ee.v1 == opposite1))
            return i;
    }

    return -1;
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
    new_vertex.edge_size = (v0.edge_size + v1.edge_size) * 0.5f;
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

        FixWinding(em, split_tri);
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
    new_vertex.selected = false;

    // Create two new triangles
    EditorFace& tri1 = em.faces[em.face_count++];
    EditorFace& tri2 = em.faces[em.face_count++];

    // Copy color from original triangle
    tri1.color = et.color;
    tri2.color = et.color;

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

    FixWinding(em, tri1);
    FixWinding(em, tri2);

    return new_vertex_index;
}

int HitTestVertex(EditorMesh& em, const Vec2& world_pos)
{
    const float size = g_view.select_size;
    float best_dist = F32_MAX;
    int best_vertex = -1;
    for (int i = 0; i < em.vertex_count; i++)
    {
        const EditorVertex& ev = em.vertices[i];
        float dist = Length(world_pos - ev.position);
        if (dist < size && dist < best_dist)
        {
            best_vertex = i;
            best_dist = dist;
        }
    }

    return best_vertex;
}

int HitTestEdge(EditorMesh& em, const Vec2& hit_pos, float* where)
{
    const float size = g_view.select_size * 0.75f;
    float best_dist = F32_MAX;
    int best_edge = -1;
    float best_where = 0.0f;
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
            if (dist < size && dist < best_dist)
            {
                best_edge = i;
                best_dist = dist;
                best_where = proj / edge_length;
            }
        }
    }

    if (where)
        *where = best_where;

    return best_edge;
}

    void Center(EditorMesh& em)
{
    Vec2 size = GetSize(em.bounds);
    Vec2 min = em.bounds.min;
    Vec2 offset = min + size * 0.5f;
    for (int i=0; i<em.vertex_count; i++)
        em.vertices[i].position = em.vertices[i].position - offset;

    UpdateEdges(em);
    MarkDirty(em);
}

bool OverlapBounds(EditorMesh& mesh, const Vec2& position, const Bounds2& hit_bounds)
{
    return Intersects(mesh.bounds + position, hit_bounds);
}

int HitTestFace(EditorMesh& em, const Vec2& position, const Vec2& hit_pos, Vec2* where)
{
    for (int i=0; i<em.face_count; i++)
    {
        EditorFace& et = em.faces[i];
        Vec2 v0 = em.vertices[et.v0].position + position;
        Vec2 v1 = em.vertices[et.v1].position + position;
        Vec2 v2 = em.vertices[et.v2].position + position;

        if (OverlapPoint(v0, v1, v2, hit_pos, where))
            return i;
    }

    return -1;
}

int AddVertex(EditorMesh& em, const Vec2& position)
{
    if (em.vertex_count >= MAX_VERTICES)
        return -1;

    // If on a vertex then return -1
    int vertex_index = HitTestVertex(em, position);
    if (vertex_index != -1)
        return -1;

    // If on an edge then split the edge and add the point
    float edge_pos;
    int edge_index = HitTestEdge(em, position, &edge_pos);
    if (edge_index >= 0)
    {
        int new_vertex = SplitEdge(em, edge_index, edge_pos);
        if (new_vertex != -1)
        {
            UpdateEdges(em);
            MarkDirty(em);
        }
        return new_vertex;
    }

    // If inside a triangle then split the triangle into three triangles and add the point
    Vec2 tri_pos;
    int triangle_index = HitTestFace(em, VEC2_ZERO, position, &tri_pos);
    if (triangle_index >= 0)
    {
        int new_vertex = SplitTriangle(em, triangle_index, position);
        if (new_vertex != -1)
        {
            UpdateEdges(em);
            MarkDirty(em);
        }
        return new_vertex;
    }

    // If outside all triangles, find the closest edge and create a triangle with it
    int closest_edge = -1;
    float closest_dist = FLT_MAX;

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
        }
    }

    // If no edge found or mesh is empty, create a standalone vertex
    if (closest_edge == -1)
        return -1;

    if (em.face_count >= MAX_TRIANGLES)
        return -1;

    // Create new vertex
    int new_vertex_index = em.vertex_count++;
    EditorVertex& new_vertex = em.vertices[new_vertex_index];
    new_vertex.position = position;
    new_vertex.height = 0.0f;
    new_vertex.selected = false;
    new_vertex.edge_size =
        (em.vertices[em.edges[closest_edge].v0].edge_size +
         em.vertices[em.edges[closest_edge].v1].edge_size) * 0.5f;

    // Create triangle with the closest edge
    const EditorEdge& ee = em.edges[closest_edge];
    EditorFace& new_triangle = em.faces[em.face_count++];
    new_triangle.v0 = ee.v0;
    new_triangle.v1 = ee.v1;
    new_triangle.v2 = new_vertex_index;
    new_triangle.color = {0, 0}; // Default color
    FixWinding(em, new_triangle);

    UpdateEdges(em);
    MarkDirty(em);
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

static void ParseVertexHeght(EditorVertex& ev, Tokenizer& tk)
{
    if (!ExpectFloat(tk, &ev.height))
        ThrowError("missing vertex height value");
}

static void ParseVertexEdge(EditorVertex& ev, Tokenizer& tk)
{
    if (!ExpectFloat(tk, &ev.edge_size))
        ThrowError("missing vertex edge value");
}

static void ParseVertex(EditorMesh& em, Tokenizer& tk)
{
    if (em.vertex_count >= MAX_VERTICES)
        ThrowError("too many vertices");

    f32 x;
    if (!ExpectFloat(tk, &x))
        ThrowError("missing vertex x coordinate");

    f32 y;
    if (!ExpectFloat(tk, &y))
        ThrowError("missing vertex y coordinate");

    EditorVertex& ev = em.vertices[em.vertex_count++];
    ev.position = {x,y};

    while (!IsEOF(tk))
    {
        if (ExpectIdentifier(tk, "e"))
            ParseVertexEdge(ev, tk);
        else if (ExpectIdentifier(tk, "h"))
            ParseVertexHeght(ev, tk);
        else
            break;
    }
}

static void ParseEdgeColor(EditorMesh& em, Tokenizer& tk)
{
    int cx;
    if (!ExpectInt(tk, &cx))
        ThrowError("missing edge color x value");

    int cy;
    if (!ExpectInt(tk, &cy))
        ThrowError("missing edge color y value");

    em.edge_color = {(u8)cx, (u8)cy};
}

static void ParseFaceColor(EditorFace& ef, Tokenizer& tk)
{
    int cx;
    if (!ExpectInt(tk, &cx))
        ThrowError("missing face color x value");

    int cy;
    if (!ExpectInt(tk, &cy))
        ThrowError("missing face color y value");

    ef.color = {(u8)cx, (u8)cy};
}

static void ParseFaceNormal(EditorFace& ef, Tokenizer& tk)
{
    f32 nx;
    if (!ExpectFloat(tk, &nx))
        ThrowError("missing face normal x value");

    f32 ny;
    if (!ExpectFloat(tk, &ny))
        ThrowError("missing face normal y value");

    f32 nz;
    if (!ExpectFloat(tk, &nz))
        ThrowError("missing face normal z value");

    ef.normal = {nx, ny, nz};
}

static void ParseFace(EditorMesh& em, Tokenizer& tk)
{
    if (em.face_count >= MAX_TRIANGLES)
        ThrowError("too many faces");

    int v0;
    if (!ExpectInt(tk, &v0))
        ThrowError("missing face v0 index");

    int v1;
    if (!ExpectInt(tk, &v1))
        ThrowError("missing face v1 index");

    int v2;
    if (!ExpectInt(tk, &v2))
        ThrowError("missing face v2 index");

    if (v0 < 0 || v0 >= em.vertex_count || v1 < 0 || v1 >= em.vertex_count || v2 < 0 || v2 >= em.vertex_count)
        ThrowError("face vertex index out of range");

    EditorFace& ef = em.faces[em.face_count++];
    ef.v0 = v0;
    ef.v1 = v1;
    ef.v2 = v2;
    ef.color = {0, 0};

    while (!IsEOF(tk))
    {
        if (ExpectIdentifier(tk, "c"))
            ParseFaceColor(ef, tk);
        else if (ExpectIdentifier(tk, "n"))
            ParseFaceNormal(ef, tk);
        else
            break;
    }
}

static EditorMesh* CreateEditableMesh(Allocator* allocator)
{
    return (EditorMesh*)Alloc(allocator, sizeof(EditorMesh));
}

EditorMesh* LoadEditorMesh(Allocator* allocator, const std::filesystem::path& path)
{
    std::string contents = ReadAllText(ALLOCATOR_DEFAULT, path);
    Tokenizer tk;
    Init(tk, contents.c_str());

    EditorMesh* em = CreateEditableMesh(allocator);

    try
    {
        while (!IsEOF(tk))
        {
            if (ExpectIdentifier(tk, "v"))
                ParseVertex(*em, tk);
            else if (ExpectIdentifier(tk, "f"))
                ParseFace(*em, tk);
            else if (ExpectIdentifier(tk, "e"))
                ParseEdgeColor(*em, tk);
            else
            {
                char error[1024];
                GetString(tk, error, sizeof(error) - 1);
                ThrowError("invalid token '%s' in mesh", error);
            }
        }
    }
    catch (std::exception& e)
    {
        LogFileError(path.string().c_str(), e.what());
        return nullptr;
    }

    Bounds2 bounds = { em->vertices[0].position, em->vertices[0].position };
    for (int i=0; i<em->vertex_count; i++)
    {
        bounds.min = Min(bounds.min, em->vertices[i].position);
        bounds.max = Max(bounds.max, em->vertices[i].position);
    }

    for (int i = 0; i<em->face_count; i++)
    {
        EditorFace& ef = em->faces[i];
        FixWinding(*em, ef);
    }

    UpdateEdges(*em);
    MarkDirty(*em);

    return em;
}

static void EditorMeshSave(EditorAsset& ea, const std::filesystem::path& path)
{
    EditorMesh& em = ea.mesh;
    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);

    WriteCSTR(stream, "e %d %d\n", em.edge_color.x, em.edge_color.y);
    WriteCSTR(stream, "\n");

    for (int i=0; i<em.vertex_count; i++)
    {
        const EditorVertex& ev = em.vertices[i];
        WriteCSTR(stream, "v %f %f e %f h %f\n", ev.position.x, ev.position.y, ev.edge_size, ev.height);
    }

    WriteCSTR(stream, "\n");

    for (int i=0; i<em.face_count; i++)
    {
        const EditorFace& ef = em.faces[i];
        WriteCSTR(stream, "f %d %d %d c %d %d n %f %f %f\n", ef.v0, ef.v1, ef.v2, ef.color.x, ef.color.y, ef.normal.x, ef.normal.y, ef.normal.z);
    }

    SaveStream(stream, path);
    Free(stream);
}

EditorAsset* NewEditorMesh(const std::filesystem::path& path)
{
    const char* default_mesh = "v -1 -1 e 1 h 0\n"
                               "v 1 -1 e 1 h 0\n"
                               "v 1 1 e 1 h 0\n"
                               "v -1 1 e 1 h 0\n"
                               "\n"
                               "f 0 1 2 c 1 0\n"
                               "f 0 2 3 c 1 0\n";

    std::string text = default_mesh;

    if (g_view.selected_asset_count == 1)
    {
        EditorAsset* selected = GetEditorAsset(GetFirstSelectedAsset());
        if (selected->type != EDITOR_ASSET_TYPE_MESH)
            return nullptr;

        text = ReadAllText(ALLOCATOR_DEFAULT, selected->path);
    }

    std::filesystem::path full_path = path.is_relative() ?  std::filesystem::current_path() / "assets" / "meshes" / path : path;
    full_path += ".mesh";

    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);
    WriteCSTR(stream, text.c_str());
    SaveStream(stream, full_path);
    Free(stream);

    return CreateEditableMeshAsset(full_path, LoadEditorMesh(ALLOCATOR_DEFAULT, full_path));
}

EditorAsset* LoadEditorMeshAsset(const std::filesystem::path& path)
{
    EditorMesh* em = LoadEditorMesh(ALLOCATOR_DEFAULT, path);
    if (!em)
        return nullptr;

    return CreateEditableMeshAsset(path, em);
}

static bool EditorMeshOverlapPoint(EditorAsset& ea, const Vec2& position, const Vec2& overlap_point)
{
    EditorMesh& em = ea.mesh;
    Mesh* mesh = ToMesh(em, false);
    if (!mesh)
        return false;

    return OverlapPoint(mesh, overlap_point - position);
}

static bool EditorMeshOverlapBounds(EditorAsset& ea, const Bounds2& overlap_bounds)
{
    return OverlapBounds(ea.mesh, ea.position, overlap_bounds);
}

static Bounds2 EditorMeshBounds(EditorAsset& ea)
{
    return ea.mesh.bounds;
}

static void EditorClone(EditorAsset& ea)
{
    ea.mesh.mesh = nullptr;
}

extern void MeshViewInit();

EditorAsset* CreateEditableMeshAsset(const std::filesystem::path& path, EditorMesh* em)
{
    EditorAsset* ea = CreateEditorAsset(ALLOCATOR_DEFAULT, path, EDITOR_ASSET_TYPE_MESH);
    ea->mesh = *em;
    ea->vtable = {
        .bounds = EditorMeshBounds,
        .draw = EditorMeshDraw,
        .view_init = MeshViewInit,
        .overlap_point = EditorMeshOverlapPoint,
        .overlap_bounds = EditorMeshOverlapBounds,
        .save = EditorMeshSave,
        .clone = EditorClone
    };
    Free(em);
    return ea;
}
