//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

constexpr float OUTLINE_WIDTH = 0.05f;

static void Init(EditorMesh* em);

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

static void EditorMeshDraw(EditorAsset* ea)
{
    assert(ea->type == EDITOR_ASSET_TYPE_MESH);
    EditorMesh* em = (EditorMesh*)ea;

    if (g_view.draw_mode == VIEW_DRAW_MODE_WIREFRAME)
    {
        BindColor(COLOR_EDGE);
        DrawEdges(em, ea->position);
    }
    else
    {
        BindColor(COLOR_WHITE);
        DrawMesh(ToMesh(em), Translate(ea->position));
    }
}

void DrawMesh(EditorMesh* em, const Mat3& transform)
{
    if (g_view.draw_mode == VIEW_DRAW_MODE_WIREFRAME)
        return;

    BindMaterial(g_view.draw_mode == VIEW_DRAW_MODE_SHADED ? g_view.shaded_material : g_view.solid_material);
    DrawMesh(ToMesh(em), transform);
}

Vec2 GetFaceCenter(EditorMesh* em, int face_index)
{
    const EditorFace& ef = em->faces[face_index];
    const EditorVertex& v0 = em->vertices[ef.v0];
    const EditorVertex& v1 = em->vertices[ef.v1];
    const EditorVertex& v2 = em->vertices[ef.v2];
    return (v0.position + v1.position + v2.position) / 3.0f;
}

bool IsVertexOnOutsideEdge(EditorMesh* em, int v0)
{
    for (int i = 0; i < em->edge_count; i++)
    {
        EditorEdge& ee = em->edges[i];
        if (ee.triangle_count == 1 && (ee.v0 == v0 || ee.v1 == v0))
            return true;
    }

    return false;
}

int GetOrAddEdge(EditorMesh* em, int v0, int v1)
{
    int fv0 = Min(v0, v1);
    int fv1 = Max(v0, v1);

    Vec2 en = Normalize(em->vertices[v1].position - em->vertices[v0].position);
    en = Vec2{en.y, -en.x};

    EditorVertex& ev0 = em->vertices[v0];
    EditorVertex& ev1 = em->vertices[v1];
    ev0.edge_normal += en;
    ev1.edge_normal += en;

    for (int i = 0; i < em->edge_count; i++)
    {
        EditorEdge& ee = em->edges[i];
        if (ee.v0 == fv0 && ee.v1 == fv1)
        {
            ee.triangle_count++;
            return i;
        }
    }

    // Not found - add it
    if (em->edge_count >= MAX_EDGES)
        return -1;

    int edge_index = em->edge_count++;
    EditorEdge& ee = em->edges[edge_index];
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

bool FixWinding(EditorMesh* em, EditorFace& ef)
{
    if (TriangleWinding(em->vertices[ef.v0].position, em->vertices[ef.v1].position, em->vertices[ef.v2].position) >= 0)
        return false;

    // Swap v1 and v2 to fix winding
    int temp = ef.v1;
    ef.v1 = ef.v2;
    ef.v2 = temp;
    return true;
}

void UpdateEdges(EditorMesh* em)
{
    em->edge_count = 0;

    for (int i = 0; i < em->vertex_count; i++)
        em->vertices[i].edge_normal = VEC2_ZERO;

    for (int i = 0; i < em->face_count; i++)
    {
        EditorFace& et = em->faces[i];
        GetOrAddEdge(em, et.v0, et.v1);
        GetOrAddEdge(em, et.v1, et.v2);
        GetOrAddEdge(em, et.v2, et.v0);
    }
}

void MarkDirty(EditorMesh* em)
{
    Free(em->mesh);
    em->mesh = nullptr;
}

Mesh* ToMesh(EditorMesh* em, bool upload)
{
    if (em->mesh)
        return em->mesh;

    MeshBuilder* builder = CreateMeshBuilder(ALLOCATOR_DEFAULT, MAX_VERTICES, MAX_INDICES);

    // Generate the mesh body
    for (int i = 0; i < em->face_count; i++)
    {
        const EditorFace& tri = em->faces[i];
        Vec2 uv_color = ColorUV(tri.color.x, tri.color.y);
        AddVertex(builder, em->vertices[tri.v0].position, tri.normal, uv_color);
        AddVertex(builder, em->vertices[tri.v1].position, tri.normal, uv_color);
        AddVertex(builder, em->vertices[tri.v2].position, tri.normal, uv_color);
        AddTriangle(builder, (u16)(i * 3), (u16)(i * 3 + 1), (u16)(i * 3 + 2));
    }

    // Generate outline
    Vec2 edge_uv = ColorUV(em->edge_color.x, em->edge_color.y);
    for (int i=0; i < em->edge_count; i++)
    {
        const EditorEdge& ee = em->edges[i];
        if (ee.triangle_count > 1)
            continue;

        const EditorVertex& v0 = em->vertices[ee.v0];
        const EditorVertex& v1 = em->vertices[ee.v1];

        if (v0.edge_size < 0.01f && v1.edge_size < 0.01f)
            continue;

        Vec3 en = {v0.edge_normal.x, v0.edge_normal.y, 0};
        for (int face_index=0; face_index < em->face_count; face_index++)
        {
            EditorFace& ef = em->faces[face_index];
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

    em->mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE, upload);
    Free(builder);

    em->bounds = GetBounds(em->mesh);

    return em->mesh;
}

void SetEdgeColor(EditorMesh* em, const Vec2Int& color)
{
    em->edge_color = color;
    MarkDirty(em);
}

void SetSelectedTrianglesColor(EditorMesh* em, const Vec2Int& color)
{
    int count = 0;
    for (i32 i = 0; i < em->face_count; i++)
    {
        EditorFace& et = em->faces[i];
        if (em->vertices[et.v0].selected && em->vertices[et.v1].selected && em->vertices[et.v2].selected)
        {
            et.color = color;
            count++;
        }
    }

    if (!count)
        return;

    MarkDirty(em);
}

static void RemoveVertex(EditorMesh* em, int vertex_index)
{
    // Shift all vertices down as long as it wasn't the last vertex
    for (int i = vertex_index; i < em->vertex_count - 1; i++)
        em->vertices[i] = em->vertices[i + 1];

    em->vertex_count--;

    // Update all triangle vertex indices that are greater than vertex_index
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
}

void MergeSelectedVerticies(EditorMesh* em)
{
    // Find all selected vertices and calculate center
    Vec2 center = VEC2_ZERO;
    int selected_indices[MAX_VERTICES];
    int selected_count = 0;
    for (int i = 0; i < em->vertex_count; i++)
    {
        if (!em->vertices[i].selected)
            continue;

        center += em->vertices[i].position;
        if (selected_count < MAX_VERTICES)
            selected_indices[selected_count++] = i;
    }

    if (selected_count <= 1)
        return;

    center = center * (1.0f / selected_count);

    // Use the first selected vertex as the merge target
    int merged_vertex_index = selected_indices[0];

    // Store original winding for each triangle before redirecting
    bool original_winding[MAX_TRIANGLES];
    for (int i = 0; i < em->face_count; i++)
    {
        EditorFace& ef = em->faces[i];
        original_winding[i] = (TriangleWinding(em->vertices[ef.v0].position, em->vertices[ef.v1].position, em->vertices[ef.v2].position) >= 0);
    }

    // Redirect all triangles that use the other selected vertices to use the merged vertex
    for (int i = 1; i < selected_count; i++)
    {
        int old_vertex_index = selected_indices[i];

        for (int face_idx = 0; face_idx < em->face_count; face_idx++)
        {
            EditorFace& ef = em->faces[face_idx];
            if (ef.v0 == old_vertex_index)
                ef.v0 = merged_vertex_index;
            if (ef.v1 == old_vertex_index)
                ef.v1 = merged_vertex_index;
            if (ef.v2 == old_vertex_index)
                ef.v2 = merged_vertex_index;
        }
    }

    // Remove duplicate/degenerate triangles and restore winding
    for (int i = em->face_count - 1; i >= 0; i--)
    {
        EditorFace& ef = em->faces[i];
        if (ef.v0 == ef.v1 || ef.v1 == ef.v2 || ef.v2 == ef.v0)
        {
            // Remove degenerate triangle
            em->faces[i] = em->faces[em->face_count - 1];
            em->face_count--;
            // Move the original winding info too
            if (i < em->face_count)
                original_winding[i] = original_winding[em->face_count];
        }
        else
        {
            // Check if winding changed and restore it if needed
            bool current_winding = (TriangleWinding(em->vertices[ef.v0].position, em->vertices[ef.v1].position, em->vertices[ef.v2].position) >= 0);
            if (current_winding != original_winding[i])
            {
                // Winding changed, fix it by swapping v1 and v2
                int temp = ef.v1;
                ef.v1 = ef.v2;
                ef.v2 = temp;
            }
        }
    }

    // Sort indices in descending order to remove from highest to lowest
    for (int i = 0; i < selected_count - 1; i++)
    {
        for (int j = i + 1; j < selected_count; j++)
        {
            if (selected_indices[i] < selected_indices[j])
            {
                int temp = selected_indices[i];
                selected_indices[i] = selected_indices[j];
                selected_indices[j] = temp;
            }
        }
    }

    // Remove the now-unused vertices (skip the first one which is our merged vertex)
    for (int i = 0; i < selected_count; i++)
    {
        if (selected_indices[i] != merged_vertex_index)
        {
            RemoveVertex(em, selected_indices[i]);

            // Adjust remaining indices since we just removed a vertex
            for (int j = i + 1; j < selected_count; j++)
            {
                if (selected_indices[j] > selected_indices[i])
                    selected_indices[j]--;
            }

            // Adjust merged vertex index if it was affected
            if (merged_vertex_index > selected_indices[i])
                merged_vertex_index--;
        }
    }

    // Now move the merged vertex to the center position
    em->vertices[merged_vertex_index].position = center;

    // Clear any accumulated edge normal for the merged vertex
    em->vertices[merged_vertex_index].edge_normal = VEC2_ZERO;

    UpdateEdges(em);
    MarkDirty(em);
}

static void UpdateRefCounts(EditorMesh* em)
{
    for (int i=0; i<em->vertex_count; i++)
        em->vertices[i].ref_count = 0;

    for (int i=0; i<em->face_count; i++)
    {
        EditorFace& ef = em->faces[i];
        em->vertices[ef.v0].ref_count++;
        em->vertices[ef.v1].ref_count++;
        em->vertices[ef.v2].ref_count++;
    }
}

static void DeleteVertex(EditorMesh* em, int vertex_index)
{
    assert(vertex_index >= 0 && vertex_index < em->vertex_count);

    for (int edge_index=em->edge_count-1; edge_index >= 0; edge_index--)
    {
        EditorEdge& ee = em->edges[edge_index];
        if (ee.v0 == vertex_index || ee.v1 == vertex_index)
        {
            em->edges[edge_index] = em->edges[--em->edge_count];
            continue;
        }
        if (ee.v0 > vertex_index) ee.v0--;
        if (ee.v1 > vertex_index) ee.v1--;
    }

    for (int face_index=em->face_count-1; face_index >= 0; face_index--)
    {
        EditorFace& ef = em->faces[face_index];
        if (ef.v0 == vertex_index || ef.v1 == vertex_index || ef.v2 == vertex_index)
        {
            em->faces[face_index] = em->faces[--em->face_count];
            continue;
        }

        if (ef.v0 > vertex_index) ef.v0--;
        if (ef.v1 > vertex_index) ef.v1--;
        if (ef.v2 > vertex_index) ef.v2--;
    }

    for (int i=vertex_index; i<em->vertex_count-1; i++)
        em->vertices[i] = em->vertices[i+1];

    em->vertex_count--;

    UpdateRefCounts(em);

    for (int i=em->vertex_count-1; i>=0; i--)
        if (em->vertices[i].ref_count == 0)
        {
            DeleteVertex(em, i);
            return;
        }

    UpdateEdges(em);
    MarkDirty(em);
}

static void DeleteFace(EditorMesh* em, int face_index)
{
    assert(face_index >= 0 && face_index < em->face_count);

    em->faces[face_index] = em->faces[em->face_count - 1];
    em->face_count--;

    MarkDirty(em);
}

static void DissolveFace(EditorMesh* em, int face_index)
{
    assert(face_index >= 0 && face_index < em->face_count);
    char vertex_count[MAX_VERTICES] = {};

    for (int i=0; i<em->face_count; i++)
    {
        if (i == face_index)
            continue;

        EditorFace& ef = em->faces[i];
        vertex_count[ef.v0] = 1;
        vertex_count[ef.v1] = 1;
        vertex_count[ef.v2] = 1;
    }

    EditorFace& df = em->faces[face_index];
    if (vertex_count[df.v0] && vertex_count[df.v1] && vertex_count[df.v2])
    {
        DeleteFace(em, face_index);
        return;
    }

    for (int vertex_index=em->vertex_count-1; vertex_index>=0; vertex_index--)
    {
        if (vertex_count[vertex_index])
            continue;

        DeleteVertex(em, vertex_index);
    }
}

void DissolveSelectedFaces(EditorMesh* em)
{
    for (int face_index=em->face_count - 1; face_index>=0; face_index--)
    {
        EditorFace& ef = em->faces[face_index];
        if (!ef.selected)
            continue;

        DissolveFace(em, face_index);
    }
}

void DissolveSelectedVertices(EditorMesh* em)
{
    for (int vertex_index=em->vertex_count - 1; vertex_index>=0; vertex_index--)
    {
        EditorVertex& ev = em->vertices[vertex_index];
        if (!ev.selected)
            continue;

        DeleteVertex(em, vertex_index);
    }
}

static float CalculateTriangleArea(const Vec2& v0, const Vec2& v1, const Vec2& v2)
{
    // Calculate signed area using cross product
    return (v1.x - v0.x) * (v2.y - v0.y) - (v2.x - v0.x) * (v1.y - v0.y);
}

int RotateEdge(EditorMesh* em, int edge_index)
{
    assert(edge_index >= 0 && edge_index < em->edge_count);

    EditorEdge& edge = em->edges[edge_index];

    // Find the two triangles that share this edge
    int triangle_indices[2];
    int triangle_count = 0;

    for (int i = 0; i < em->face_count; i++)
    {
        EditorFace& et = em->faces[i];
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

    EditorFace& f1 = em->faces[triangle_indices[0]];
    EditorFace& f2 = em->faces[triangle_indices[1]];

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

    float f1a = fabsf(CalculateTriangleArea(em->vertices[f1n.v0].position, em->vertices[f1n.v1].position, em->vertices[f1n.v2].position));
    float f2a = fabsf(CalculateTriangleArea(em->vertices[f2n.v0].position, em->vertices[f2n.v1].position, em->vertices[f2n.v2].position));
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

    UpdateEdges(em);
    MarkDirty(em);

    // find the new edge index
    for (int i=0; i<em->edge_count; i++)
    {
        const EditorEdge& ee = em->edges[i];
        if ((ee.v0 == opposite1 && ee.v1 == opposite2) || (ee.v0 == opposite2 && ee.v1 == opposite1))
            return i;
    }

    return -1;
}

int SplitEdge(EditorMesh* em, int edge_index, float edge_pos)
{
    assert(edge_index >= 0 && edge_index < em->edge_count);

    if (em->vertex_count >= MAX_VERTICES)
        return -1;

    if (em->face_count + 2 >= MAX_TRIANGLES)
        return -1;

    EditorEdge& ee = em->edges[edge_index];
    EditorVertex& v0 = em->vertices[ee.v0];
    EditorVertex& v1 = em->vertices[ee.v1];

    int new_vertex_index = em->vertex_count++;
    EditorVertex& new_vertex = em->vertices[new_vertex_index];
    new_vertex.edge_size = (v0.edge_size + v1.edge_size) * 0.5f;
    new_vertex.position = (v0.position * (1.0f - edge_pos) + v1.position * edge_pos);

    int triangle_count = em->face_count;
    for (int i = 0; i < triangle_count; i++)
    {
        EditorFace& et = em->faces[i];

        int triangle_edge = GetTriangleEdgeIndex(et, ee);
        if (triangle_edge == -1)
            continue;

        EditorFace& split_tri = em->faces[em->face_count++];
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

int SplitTriangle(EditorMesh* em, int triangle_index, const Vec2& position)
{
    assert(triangle_index >= 0 && triangle_index < em->face_count);

    if (em->vertex_count >= MAX_VERTICES)
        return -1;

    if (em->face_count + 2 >= MAX_TRIANGLES)
        return -1;

    EditorFace& et = em->faces[triangle_index];

    // Create new vertex at the position
    int new_vertex_index = em->vertex_count++;
    EditorVertex& new_vertex = em->vertices[new_vertex_index];
    new_vertex.position = position;
    new_vertex.height = 0.0f;
    new_vertex.selected = false;

    // Create two new triangles
    EditorFace& tri1 = em->faces[em->face_count++];
    EditorFace& tri2 = em->faces[em->face_count++];

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

int HitTestVertex(EditorMesh* em, const Vec2& world_pos, float size_mult)
{
    float size = g_view.select_size * size_mult;
    float best_dist = F32_MAX;
    int best_vertex = -1;
    for (int i = 0; i < em->vertex_count; i++)
    {
        const EditorVertex& ev = em->vertices[i];
        float dist = Length(world_pos - ev.position);
        if (dist < size && dist < best_dist)
        {
            best_vertex = i;
            best_dist = dist;
        }
    }

    return best_vertex;
}

int HitTestEdge(EditorMesh* em, const Vec2& hit_pos, float* where)
{
    const float size = g_view.select_size * 0.75f;
    float best_dist = F32_MAX;
    int best_edge = -1;
    float best_where = 0.0f;
    for (int i = 0; i < em->edge_count; i++)
    {
        const EditorEdge& ee = em->edges[i];
        const Vec2& v0 = em->vertices[ee.v0].position;
        const Vec2& v1 = em->vertices[ee.v1].position;
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

    void Center(EditorMesh* em)
{
    Vec2 size = GetSize(em->bounds);
    Vec2 min = em->bounds.min;
    Vec2 offset = min + size * 0.5f;
    for (int i=0; i<em->vertex_count; i++)
        em->vertices[i].position = em->vertices[i].position - offset;

    UpdateEdges(em);
    MarkDirty(em);
}

bool OverlapBounds(EditorMesh* em, const Vec2& position, const Bounds2& hit_bounds)
{
    return Intersects(em->bounds + position, hit_bounds);
}

int HitTestFace(EditorMesh* em, const Vec2& position, const Vec2& hit_pos, Vec2* where)
{
    for (int i=0; i<em->face_count; i++)
    {
        EditorFace& et = em->faces[i];
        Vec2 v0 = em->vertices[et.v0].position + position;
        Vec2 v1 = em->vertices[et.v1].position + position;
        Vec2 v2 = em->vertices[et.v2].position + position;

        if (OverlapPoint(v0, v1, v2, hit_pos, where))
            return i;
    }

    return -1;
}

int AddVertex(EditorMesh* em, const Vec2& position)
{
    if (em->vertex_count >= MAX_VERTICES)
        return -1;

    // If on a vertex then return -1
    int vertex_index = HitTestVertex(em, position, 0.1f);
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

    for (int i = 0; i < em->edge_count; i++)
    {
        const EditorEdge& ee = em->edges[i];
        const Vec2& v0 = em->vertices[ee.v0].position;
        const Vec2& v1 = em->vertices[ee.v1].position;

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

    if (em->face_count >= MAX_TRIANGLES)
        return -1;

    // Create new vertex
    int new_vertex_index = em->vertex_count++;
    EditorVertex& new_vertex = em->vertices[new_vertex_index];
    new_vertex.position = position;
    new_vertex.height = 0.0f;
    new_vertex.selected = false;
    new_vertex.edge_size =
        (em->vertices[em->edges[closest_edge].v0].edge_size +
         em->vertices[em->edges[closest_edge].v1].edge_size) * 0.5f;

    // Create triangle with the closest edge
    const EditorEdge& ee = em->edges[closest_edge];
    EditorFace& new_triangle = em->faces[em->face_count++];
    new_triangle.v0 = ee.v0;
    new_triangle.v1 = ee.v1;
    new_triangle.v2 = new_vertex_index;
    new_triangle.color = {0, 0}; // Default color
    FixWinding(em, new_triangle);

    UpdateEdges(em);
    MarkDirty(em);
    return new_vertex_index;
}

void FixNormals(EditorMesh* em)
{
    for (int i=0; i<em->face_count; i++)
    {
        // Ensure all triangles have CCW winding
        EditorFace& et = em->faces[i];
        const Vec2& v0 = em->vertices[et.v0].position;
        const Vec2& v1 = em->vertices[et.v1].position;
        const Vec2& v2 = em->vertices[et.v2].position;

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

static void ParseVertex(EditorMesh* em, Tokenizer& tk)
{
    if (em->vertex_count >= MAX_VERTICES)
        ThrowError("too many vertices");

    f32 x;
    if (!ExpectFloat(tk, &x))
        ThrowError("missing vertex x coordinate");

    f32 y;
    if (!ExpectFloat(tk, &y))
        ThrowError("missing vertex y coordinate");

    EditorVertex& ev = em->vertices[em->vertex_count++];
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

static void ParseEdgeColor(EditorMesh* em, Tokenizer& tk)
{
    int cx;
    if (!ExpectInt(tk, &cx))
        ThrowError("missing edge color x value");

    int cy;
    if (!ExpectInt(tk, &cy))
        ThrowError("missing edge color y value");

    em->edge_color = {(u8)cx, (u8)cy};
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

static void ParseFace(EditorMesh* em, Tokenizer& tk)
{
    if (em->face_count >= MAX_TRIANGLES)
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

    if (v0 < 0 || v0 >= em->vertex_count || v1 < 0 || v1 >= em->vertex_count || v2 < 0 || v2 >= em->vertex_count)
        ThrowError("face vertex index out of range");

    EditorFace& ef = em->faces[em->face_count++];
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

EditorMesh* LoadEditorMesh(const std::filesystem::path& path)
{
    std::string contents = ReadAllText(ALLOCATOR_DEFAULT, path);
    Tokenizer tk;
    Init(tk, contents.c_str());

    EditorMesh* em = (EditorMesh*)CreateEditorAsset(EDITOR_ASSET_TYPE_MESH, path);
    assert(em);
    Init(em);

    try
    {
        while (!IsEOF(tk))
        {
            if (ExpectIdentifier(tk, "v"))
                ParseVertex(em, tk);
            else if (ExpectIdentifier(tk, "f"))
                ParseFace(em, tk);
            else if (ExpectIdentifier(tk, "e"))
                ParseEdgeColor(em, tk);
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
        FixWinding(em, ef);
    }

    ToMesh(em, false);
    UpdateEdges(em);
    MarkDirty(em);

    return em;
}

static void EditorMeshSave(EditorAsset* ea, const std::filesystem::path& path)
{
    assert(ea->type == EDITOR_ASSET_TYPE_MESH);
    EditorMesh* em = (EditorMesh*)ea;
    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);

    WriteCSTR(stream, "e %d %d\n", em->edge_color.x, em->edge_color.y);
    WriteCSTR(stream, "\n");

    for (int i=0; i<em->vertex_count; i++)
    {
        const EditorVertex& ev = em->vertices[i];
        WriteCSTR(stream, "v %f %f e %f h %f\n", ev.position.x, ev.position.y, ev.edge_size, ev.height);
    }

    WriteCSTR(stream, "\n");

    for (int i=0; i<em->face_count; i++)
    {
        const EditorFace& ef = em->faces[i];
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

    return LoadEditorMesh(full_path);
}

static bool EditorMeshOverlapPoint(EditorAsset* ea, const Vec2& position, const Vec2& overlap_point)
{
    assert(ea->type == EDITOR_ASSET_TYPE_MESH);
    EditorMesh* em = (EditorMesh*)&ea;
    Mesh* mesh = ToMesh(em, false);
    if (!mesh)
        return false;

    return OverlapPoint(mesh, overlap_point - position);
}

static bool EditorMeshOverlapBounds(EditorAsset* ea, const Bounds2& overlap_bounds)
{
    assert(ea->type == EDITOR_ASSET_TYPE_MESH);
    return OverlapBounds((EditorMesh*)ea, ea->position, overlap_bounds);
}

static Bounds2 EditorMeshBounds(EditorAsset* ea)
{
    assert(ea->type == EDITOR_ASSET_TYPE_MESH);
    return ((EditorMesh*)ea)->bounds;
}

static void EditorClone(EditorAsset* ea)
{
    assert(ea->type == EDITOR_ASSET_TYPE_MESH);
    ((EditorMesh*)ea)->mesh = nullptr;
}

static void Init(EditorMesh* em)
{
    extern void MeshViewInit();

    em->vtable = {
        .bounds = EditorMeshBounds,
        .draw = EditorMeshDraw,
        .view_init = MeshViewInit,
        .overlap_point = EditorMeshOverlapPoint,
        .overlap_bounds = EditorMeshOverlapBounds,
        .save = EditorMeshSave,
        .clone = EditorClone
    };
}
