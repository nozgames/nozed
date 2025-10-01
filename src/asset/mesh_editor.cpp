//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

constexpr float OUTLINE_WIDTH = 0.05f;

static void Init(EditorMesh* em);
static void DeleteFaceInternal(EditorMesh* em, int face_index);
static void RemoveFaceVertices(EditorMesh* em, int face_index, int remove_at, int remove_count);
static void InsertFaceVertices(EditorMesh* em, int face_index, int insert_at, int count);
static void DeleteUnreferencedVertices(EditorMesh* em);
static void MergeFaces(EditorMesh* em, const EditorEdge& shared_edge);
static void DeleteFace(EditorMesh* em, int face_index);
static void DeleteVertex(EditorMesh* em, int vertex_index);

static int GetFaceEdgeIndex(EditorMesh* em, const EditorFace& ef, const EditorEdge& ee)
{
    for (int vertex_index=0; vertex_index<ef.vertex_count; vertex_index++)
    {
        int v0 = em->face_vertices[ef.vertex_offset + vertex_index];
        int v1 = em->face_vertices[ef.vertex_offset + (vertex_index + 1) % ef.vertex_count];

        if (ee.v0 == v0 && ee.v1 == v1 || ee.v0 == v1 && ee.v1 == v0)
            return vertex_index;
    }

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
        DrawMesh(em, Translate(ea->position));
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
    Vec2 center = VEC2_ZERO;

    for (int i = 0; i < ef.vertex_count; i++)
    {
        int vertex_idx = em->face_vertices[ef.vertex_offset + i];
        center += em->vertices[vertex_idx].position;
    }

    return center / (float)ef.vertex_count;
}

bool IsVertexOnOutsideEdge(EditorMesh* em, int v0)
{
    for (int i = 0; i < em->edge_count; i++)
    {
        EditorEdge& ee = em->edges[i];
        if (ee.face_count == 1 && (ee.v0 == v0 || ee.v1 == v0))
            return true;
    }

    return false;
}

static int GetEdge(EditorMesh* em, int v0, int v1)
{
    int fv0 = Min(v0, v1);
    int fv1 = Max(v0, v1);
   for (int i = 0; i < em->edge_count; i++)
    {
        EditorEdge& ee = em->edges[i];
        if (ee.v0 == fv0 && ee.v1 == fv1)
            return i;
    }

    return -1;
}

int GetOrAddEdge(EditorMesh* em, int v0, int v1, int face_index)
{
    int fv0 = Min(v0, v1);
    int fv1 = Max(v0, v1);

    for (int i = 0; i < em->edge_count; i++)
    {
        EditorEdge& ee = em->edges[i];
        if (ee.v0 == fv0 && ee.v1 == fv1)
        {
            if (ee.face_index[0] > face_index)
            {
                int temp = ee.face_index[0];
                ee.face_index[0] = face_index;
                ee.face_index[1] = temp;
            }
            else
                ee.face_index[ee.face_count] = face_index;

            ee.face_count++;
            return i;
        }
    }

    // Not found - add it
    if (em->edge_count >= MAX_EDGES)
        return -1;

    int edge_index = em->edge_count++;
    EditorEdge& ee = em->edges[edge_index];
    ee.face_count = 1;
    ee.face_index[0] = face_index;
    ee.v0 = fv0;
    ee.v1 = fv1;
    ee.normal = Normalize(-Perpendicular(em->vertices[v1].position - em->vertices[v0].position));

    return edge_index;
}

void UpdateEdges(EditorMesh* em)
{
    em->edge_count = 0;

    for (int i = 0; i < em->vertex_count; i++)
    {
        em->vertices[i].edge_normal = VEC2_ZERO;
        em->vertices[i].ref_count = 0;
    }

    for (int face_index = 0; face_index < em->face_count; face_index++)
    {
        EditorFace& ef = em->faces[face_index];

        for (int vertex_index = 0; vertex_index<ef.vertex_count - 1; vertex_index++)
        {
            int v0 = em->face_vertices[ef.vertex_offset + vertex_index];
            int v1 = em->face_vertices[ef.vertex_offset + vertex_index + 1];
            GetOrAddEdge(em, v0, v1, face_index);
        }

        int vs = em->face_vertices[ef.vertex_offset + ef.vertex_count - 1];
        int ve = em->face_vertices[ef.vertex_offset];
        GetOrAddEdge(em, vs, ve, face_index);
    }

    for (int edge_index=0; edge_index<em->edge_count; edge_index++)
    {
        EditorEdge& ee = em->edges[edge_index];
        if (ee.face_count != 1)
            continue;
        em->vertices[ee.v0].ref_count++;
        em->vertices[ee.v1].ref_count++;
        em->vertices[ee.v0].edge_normal += ee.normal;
        em->vertices[ee.v1].edge_normal += ee.normal;
    }

    for (int vertex_index=0; vertex_index<em->vertex_count; vertex_index++)
    {
        EditorVertex& ev = em->vertices[vertex_index];
        if (Length(ev.edge_normal) > F32_EPSILON)
            ev.edge_normal = Normalize(ev.edge_normal);
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
        TriangulateFace(em, em->faces + i, builder);

    // Generate outline
    Vec2 edge_uv = ColorUV(em->edge_color.x, em->edge_color.y);
    for (int i=0; i < em->edge_count; i++)
    {
        const EditorEdge& ee = em->edges[i];
        if (ee.face_count > 1)
            continue;

        const EditorVertex& v0 = em->vertices[ee.v0];
        const EditorVertex& v1 = em->vertices[ee.v1];

        if (v0.edge_size < 0.01f && v1.edge_size < 0.01f)
            continue;

        Vec3 en = VEC3_ZERO;
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
        if (et.selected)
        {
            et.color = color;
            count++;
        }
    }

    if (!count)
        return;

    MarkDirty(em);
}

static int CountSharedEdges(EditorMesh* em, int face_index0, int face_index1)
{
    assert(face_index0 < face_index1);

    int shared_edge_count = 0;
    for (int edge_index=0; edge_index<em->edge_count; edge_index++)
    {
        EditorEdge& ee = em->edges[edge_index];
        if (ee.face_count != 2)
            continue;

        if (ee.face_index[0] == face_index0 && ee.face_index[1] == face_index1)
            shared_edge_count++;
    }

    return shared_edge_count;
}

static void CollapseEdge(EditorMesh* em, int edge_index)
{
    assert(em);
    assert(edge_index >= 0 && edge_index < em->edge_count);

    EditorEdge& ee = em->edges[edge_index];
    EditorVertex& v0 = em->vertices[ee.v0];
    EditorVertex& v1 = em->vertices[ee.v1];

    DeleteVertex(em, v0.ref_count > v1.ref_count ? ee.v1 : ee.v0);
}

void DissolveEdge(EditorMesh* em, int edge_index)
{
    // todo: count the number of edges shared between the two faces for the given edge
    // todo: if the count is 1 then we can dissolve the edge
    // todo: if the count is more than 1 then we merge the two vertices

    EditorEdge& ee = em->edges[edge_index];
    assert(ee.face_count > 0);

    if (ee.face_count == 1)
    {
        EditorFace& ef = em->faces[ee.face_index[0]];
        if (ef.vertex_count <= 3)
        {
            DeleteFace(em, ee.face_index[0]);
            return;
        }

        CollapseEdge(em, edge_index);
        return;
    }

    int shared_edge_count = CountSharedEdges (em, ee.face_index[0], ee.face_index[1]);
    if (shared_edge_count == 1)
    {
        MergeFaces(em, ee);
        return;
    }

    CollapseEdge(em, edge_index);
}

static void DeleteUnreferencedVertices(EditorMesh* em)
{
    int vertex_mapping[MAX_VERTICES];
    int new_vertex_count = 0;

    for (int i = 0; i < em->vertex_count; i++)
    {
        if (em->vertices[i].ref_count <= 0)
        {
            vertex_mapping[i] = -1;
            continue;
        }

        vertex_mapping[i] = new_vertex_count;
        em->vertices[new_vertex_count++] = em->vertices[i];
    }

    if (em->vertex_count == new_vertex_count)
        return;

    em->vertex_count = new_vertex_count;

    for (int i = 0; i < em->face_vertex_count; i++)
    {
        int old_vertex_index = em->face_vertices[i];
        int new_vertex_index = vertex_mapping[old_vertex_index];
        assert(new_vertex_index != -1);
        em->face_vertices[i] = new_vertex_index;
    }

    UpdateEdges(em);
}

static void DeleteVertex(EditorMesh* em, int vertex_index) {
    assert(vertex_index >= 0 && vertex_index < em->vertex_count);

    for (int face_index=em->face_count-1; face_index >= 0; face_index--) {
        EditorFace& ef = em->faces[face_index];
        int vertex_pos = -1;
        for (int face_vertex_index=0; face_vertex_index<ef.vertex_count; face_vertex_index++) {
            if (em->face_vertices[ef.vertex_offset + face_vertex_index] == vertex_index) {
                vertex_pos = face_vertex_index;
                break;
            }
        }

        if (vertex_pos == -1)
            continue;

        if (ef.vertex_count <= 3)
            DeleteFaceInternal(em, face_index);
        else
            RemoveFaceVertices(em, face_index, vertex_pos, 1);
    }

    UpdateEdges(em);
    DeleteUnreferencedVertices(em);
    MarkDirty(em);
}

static void DeleteFaceInternal(EditorMesh* em, int face_index)
{
    assert(face_index >= 0 && face_index < em->face_count);

    RemoveFaceVertices(em, face_index, 0, -1);

    em->faces[face_index] = em->faces[em->face_count - 1];
    em->face_count--;
}

static void DeleteFace(EditorMesh* em, int face_index)
{
    DeleteFaceInternal(em, face_index);
    UpdateEdges(em);
    DeleteUnreferencedVertices(em);
    MarkDirty(em);
}

void DissolveSelectedFaces(EditorMesh* em)
{
    for (int face_index=em->face_count - 1; face_index>=0; face_index--)
    {
        EditorFace& ef = em->faces[face_index];
        if (!ef.selected)
            continue;

        DeleteFace(em, face_index);
    }
}

static void MergeFaces(EditorMesh* em, const EditorEdge& shared_edge)
{
    assert(shared_edge.face_count == 2);
    assert(CountSharedEdges(em, shared_edge.face_index[0], shared_edge.face_index[1]) == 1);

    EditorFace& face0 = em->faces[shared_edge.face_index[0]];
    EditorFace& face1 = em->faces[shared_edge.face_index[1]];

    int edge_pos0 = GetFaceEdgeIndex(em, face0, shared_edge);
    int edge_pos1 = GetFaceEdgeIndex(em, face1, shared_edge);
    assert(edge_pos0 != -1);
    assert(edge_pos1 != -1);

    int insert_pos = (edge_pos0 + 1) % face0.vertex_count;
    InsertFaceVertices(em, shared_edge.face_index[0], insert_pos, face1.vertex_count - 2);

    for (int i=0; i<face1.vertex_count - 2; i++)
        em->face_vertices[face0.vertex_offset + insert_pos + i] =
            em->face_vertices[face1.vertex_offset + ((edge_pos1 + 2 + i) % face1.vertex_count)];

    DeleteFaceInternal(em, shared_edge.face_index[1]);
    UpdateEdges(em);
    MarkDirty(em);
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

static void InsertFaceVertices(EditorMesh* em, int face_index, int insert_at, int count)
{
    EditorFace& ef = em->faces[face_index];

    em->face_vertex_count += count;

    int vertex_end = ef.vertex_offset + insert_at;
    for (int vertex_index=em->face_vertex_count-1; vertex_index > vertex_end; vertex_index--)
        em->face_vertices[vertex_index] = em->face_vertices[vertex_index-count];

    for (int i=0; i<count; i++)
        em->face_vertices[vertex_end + i] = -1;

    for (int face_index2=0; face_index2<em->face_count; face_index2++)
    {
        EditorFace& ef2 = em->faces[face_index2];
        if (ef2.vertex_offset > ef.vertex_offset)
            ef2.vertex_offset += count;
    }

    ef.vertex_count += count;
}

static void RemoveFaceVertices(EditorMesh* em, int face_index, int remove_at, int remove_count)
{
    EditorFace& ef = em->faces[face_index];
    if (remove_count == -1)
        remove_count = ef.vertex_count - remove_at;

    assert(remove_at >= 0 && remove_at + remove_count <= ef.vertex_count);

    for (int vertex_index=ef.vertex_offset + remove_at; vertex_index + remove_count < em->face_vertex_count; vertex_index++)
        em->face_vertices[vertex_index] = em->face_vertices[vertex_index + remove_count];

    ef.vertex_count -= remove_count;
    em->face_vertex_count -= remove_count;

    for (int face_index2=0; face_index2<em->face_count; face_index2++)
    {
        EditorFace& ef2 = em->faces[face_index2];
        if (ef2.vertex_offset > ef.vertex_offset)
            ef2.vertex_offset -= remove_count;
    }
}

int CreateFace(EditorMesh* em)
{
    // Collect selected vertices
    int selected_vertices[MAX_VERTICES];
    int selected_count = 0;

    for (int i = 0; i < em->vertex_count; i++)
    {
        if (em->vertices[i].selected)
        {
            if (selected_count >= MAX_VERTICES)
                return -1;
            selected_vertices[selected_count++] = i;
        }
    }

    // Need at least 3 vertices to create a face
    if (selected_count < 3)
        return -1;

    // Check if we have room for the new face
    if (em->face_count >= MAX_FACES)
        return -1;

    if (em->face_vertex_count + selected_count > MAX_INDICES)
        return -1;

    // Verify that none of the edges between selected vertices are already part of two faces
    for (int i = 0; i < selected_count; i++)
    {
        int v0 = selected_vertices[i];
        int v1 = selected_vertices[(i + 1) % selected_count];

        int edge_index = GetEdge(em, v0, v1);
        if (edge_index != -1)
        {
            const EditorEdge& ee = em->edges[edge_index];
            if (ee.face_count >= 2)
                return -1;  // Edge already belongs to two faces
        }
    }

    // Calculate centroid of selected vertices
    Vec2 centroid = VEC2_ZERO;
    for (int i = 0; i < selected_count; i++)
        centroid += em->vertices[selected_vertices[i]].position;
    centroid = centroid / (float)selected_count;

    // Sort vertices by angle around centroid to determine correct winding order
    struct VertexAngle
    {
        int vertex_index;
        float angle;
    };

    VertexAngle vertex_angles[MAX_VERTICES];
    for (int i = 0; i < selected_count; i++)
    {
        Vec2 dir = em->vertices[selected_vertices[i]].position - centroid;
        vertex_angles[i].vertex_index = selected_vertices[i];
        vertex_angles[i].angle = atan2f(dir.y, dir.x);
    }

    // Sort by angle (counter-clockwise)
    for (int i = 0; i < selected_count - 1; i++)
    {
        for (int j = i + 1; j < selected_count; j++)
        {
            if (vertex_angles[i].angle > vertex_angles[j].angle)
            {
                VertexAngle temp = vertex_angles[i];
                vertex_angles[i] = vertex_angles[j];
                vertex_angles[j] = temp;
            }
        }
    }

    // Create the face with sorted vertices
    int face_index = em->face_count++;
    EditorFace& ef = em->faces[face_index];
    ef.vertex_offset = em->face_vertex_count;
    ef.vertex_count = selected_count;
    ef.color = {1, 0};  // Default color
    ef.normal = {0, 0, 1};  // Default normal
    ef.selected = false;

    // Add vertices in counter-clockwise order
    for (int i = 0; i < selected_count; i++)
        em->face_vertices[em->face_vertex_count++] = vertex_angles[i].vertex_index;

    // Update edges
    UpdateEdges(em);
    MarkDirty(em);

    return face_index;
}

int SplitFaces(EditorMesh* em, int v0, int v1)
{
    if (em->face_count >= MAX_FACES)
        return -1;

    if (GetEdge(em, v0, v1) != -1)
        return -1;

    int face_index = 0;
    int v0_pos = -1;
    int v1_pos = -1;
    for (; face_index < em->face_count; face_index++)
    {
        EditorFace& ef = em->faces[face_index];

        v0_pos = -1;
        v1_pos = -1;
        for (int i = 0; i < ef.vertex_count && (v0_pos == -1 || v1_pos == -1); i++)
        {
            int vertex_index = em->face_vertices[ef.vertex_offset + i];
            if (vertex_index == v0) v0_pos = i;
            if (vertex_index == v1) v1_pos = i;
        }

        if (v0_pos != -1 && v1_pos != -1)
            break;
    }

    if (face_index >= em->face_count)
        return -1;

    if (v0_pos > v1_pos)
    {
        int temp = v0_pos;
        v0_pos = v1_pos;
        v1_pos = temp;
    }

    EditorFace& old_face = em->faces[face_index];
    EditorFace& new_face = em->faces[em->face_count++];
    new_face.color = old_face.color;
    new_face.normal = old_face.normal;
    new_face.selected = old_face.selected;

    int old_vertex_count = old_face.vertex_count - (v1_pos - v0_pos - 1);
    int new_vertex_count = v1_pos - v0_pos + 1;

    new_face.vertex_count = new_vertex_count;
    new_face.vertex_offset = em->face_vertex_count;
    em->face_vertex_count += new_vertex_count;
    for (int vertex_index=0; vertex_index<new_vertex_count; vertex_index++)
        em->face_vertices[new_face.vertex_offset + vertex_index] = em->face_vertices[old_face.vertex_offset + v0_pos + vertex_index];

    for (int vertex_index=0; v1_pos+vertex_index<old_face.vertex_count; vertex_index++)
        em->face_vertices[old_face.vertex_offset + v0_pos + vertex_index + 1] =
            em->face_vertices[old_face.vertex_offset + v1_pos + vertex_index];

    RemoveFaceVertices(em, face_index, old_vertex_count, old_face.vertex_count - old_vertex_count);

    UpdateEdges(em);
    MarkDirty(em);

    return GetEdge(em, em->face_vertices[old_face.vertex_offset + v0_pos], em->face_vertices[old_face.vertex_offset + (v0_pos + 1) % old_face.vertex_count]);
}

int SplitEdge(EditorMesh* em, int edge_index, float edge_pos)
{
    assert(edge_index >= 0 && edge_index < em->edge_count);

    if (em->vertex_count >= MAX_VERTICES)
        return -1;

    if (em->face_count + 2 >= MAX_FACES)
        return -1;

    EditorEdge& ee = em->edges[edge_index];
    EditorVertex& v0 = em->vertices[ee.v0];
    EditorVertex& v1 = em->vertices[ee.v1];

    int new_vertex_index = em->vertex_count++;
    EditorVertex& new_vertex = em->vertices[new_vertex_index];
    new_vertex.edge_size = (v0.edge_size + v1.edge_size) * 0.5f;
    new_vertex.position = (v0.position * (1.0f - edge_pos) + v1.position * edge_pos);

    int face_count = em->face_count;
    for (int face_index = 0; face_index < face_count; face_index++)
    {
        EditorFace& ef = em->faces[face_index];

        int face_edge = GetFaceEdgeIndex(em, ef, ee);
        if (face_edge == -1)
            continue;

        InsertFaceVertices(em, face_index, face_edge + 1, 1);
        em->face_vertices[ef.vertex_offset + face_edge + 1] = new_vertex_index;
    }

    UpdateEdges(em);
    MarkDirty(em);

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
    for (int i = em->face_count - 1; i >= 0; i--)
    {
        EditorFace& ef = em->faces[i];

        // Ray casting algorithm - works for both convex and concave polygons
        int intersections = 0;

        for (int vertex_index = 0; vertex_index < ef.vertex_count; vertex_index++)
        {
            int v0_idx = em->face_vertices[ef.vertex_offset + vertex_index];
            int v1_idx = em->face_vertices[ef.vertex_offset + (vertex_index + 1) % ef.vertex_count];

            Vec2 v0 = em->vertices[v0_idx].position + position;
            Vec2 v1 = em->vertices[v1_idx].position + position;

            // Cast horizontal ray to the right from hit_pos
            // Check if this edge intersects the ray
            float min_y = Min(v0.y, v1.y);
            float max_y = Max(v0.y, v1.y);

            // Skip horizontal edges and edges that don't cross the ray's Y level
            if (hit_pos.y < min_y || hit_pos.y >= max_y || min_y == max_y)
                continue;

            // Calculate X intersection point
            float t = (hit_pos.y - v0.y) / (v1.y - v0.y);
            float x_intersect = v0.x + t * (v1.x - v0.x);

            // Count intersection if it's to the right of the point
            if (x_intersect > hit_pos.x)
                intersections++;
        }

        // Point is inside if odd number of intersections
        bool inside = (intersections % 2) == 1;

        if (inside)
        {
            // Calculate barycentric coordinates for the first triangle if needed
            if (where && ef.vertex_count >= 3)
            {
                Vec2 v0 = em->vertices[em->face_vertices[ef.vertex_offset + 0]].position + position;
                Vec2 v1 = em->vertices[em->face_vertices[ef.vertex_offset + 1]].position + position;
                Vec2 v2 = em->vertices[em->face_vertices[ef.vertex_offset + 2]].position + position;

                Vec2 v0v1 = v1 - v0;
                Vec2 v0v2 = v2 - v0;
                Vec2 v0p = hit_pos - v0;

                float dot00 = Dot(v0v2, v0v2);
                float dot01 = Dot(v0v2, v0v1);
                float dot02 = Dot(v0v2, v0p);
                float dot11 = Dot(v0v1, v0v1);
                float dot12 = Dot(v0v1, v0p);

                float inv_denom = 1.0f / (dot00 * dot11 - dot01 * dot01);
                float u = (dot11 * dot02 - dot01 * dot12) * inv_denom;
                float v = (dot00 * dot12 - dot01 * dot02) * inv_denom;

                *where = Vec2{u, v};
            }

            return i;
        }
    }

    return -1;
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
    if (em->face_count >= MAX_FACES)
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

    EditorFace& ef = em->faces[em->face_count++];

    ef.vertex_offset = em->face_vertex_count;
    em->face_vertices[em->face_vertex_count++] = v0;
    em->face_vertices[em->face_vertex_count++] = v1;
    em->face_vertices[em->face_vertex_count++] = v2;

    while (ExpectInt(tk, &v2))
        em->face_vertices[em->face_vertex_count++] = v2;

    ef.vertex_count = em->face_vertex_count - ef.vertex_offset;

    if (v0 < 0 || v0 >= em->vertex_count || v1 < 0 || v1 >= em->vertex_count || v2 < 0 || v2 >= em->vertex_count)
        ThrowError("face vertex index out of range");

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

    EditorMesh* em = (EditorMesh*)CreateEditorAsset(path);
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

    ToMesh(em, false);
    UpdateEdges(em);
    MarkDirty(em);

    return em;
}

void EditorMeshLoad(EditorAsset* ea)
{
    assert(ea);
    assert(ea->type == EDITOR_ASSET_TYPE_MESH);
    EditorMesh* em = (EditorMesh*)ea;

    std::string contents = ReadAllText(ALLOCATOR_DEFAULT, ea->path);
    Tokenizer tk;
    Init(tk, contents.c_str());

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

    Bounds2 bounds = { em->vertices[0].position, em->vertices[0].position };
    for (int i=0; i<em->vertex_count; i++)
    {
        bounds.min = Min(bounds.min, em->vertices[i].position);
        bounds.max = Max(bounds.max, em->vertices[i].position);
    }

    ToMesh(em, false);
    UpdateEdges(em);
    MarkDirty(em);
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

        WriteCSTR(stream, "f ");
        for (int vertex_index=0; vertex_index<ef.vertex_count; vertex_index++)
        {
            int v = em->face_vertices[ef.vertex_offset + vertex_index];
            WriteCSTR(stream, " %d", v);
        }

        WriteCSTR(stream, " c %d %d n %f %f %f\n", ef.color.x, ef.color.y, ef.normal.x, ef.normal.y, ef.normal.z);
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
                               "f 0 1 2 3 c 1 0\n";

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
    EditorMesh* em = (EditorMesh*)ea;
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

static void EditorClone(EditorAsset* ea)
{
    assert(ea->type == EDITOR_ASSET_TYPE_MESH);
    ((EditorMesh*)ea)->mesh = nullptr;
}

static void Init(EditorMesh* em)
{
    extern void MeshViewInit();

    em->vtable = {
        .load = EditorMeshLoad,
        .save = EditorMeshSave,
        .draw = EditorMeshDraw,
        .view_init = MeshViewInit,
        .overlap_point = EditorMeshOverlapPoint,
        .overlap_bounds = EditorMeshOverlapBounds,
        .clone = EditorClone
    };
}

void InitEditorMesh(EditorAsset* ea)
{
    assert(ea);
    assert(ea->type == EDITOR_ASSET_TYPE_MESH);
    EditorMesh* em = (EditorMesh*)ea;
    Init(em);
}

static bool IsEar(EditorMesh* em, int* indices, int vertex_count, int ear_index)
{
    int prev = (ear_index - 1 + vertex_count) % vertex_count;
    int curr = ear_index;
    int next = (ear_index + 1) % vertex_count;

    Vec2 v0 = em->vertices[indices[prev]].position;
    Vec2 v1 = em->vertices[indices[curr]].position;
    Vec2 v2 = em->vertices[indices[next]].position;

    // Check if triangle has correct winding (counter-clockwise)
    float cross = (v1.x - v0.x) * (v2.y - v0.y) - (v2.x - v0.x) * (v1.y - v0.y);
    if (cross <= 0)
        return false;

    // Check if any other vertex is inside this triangle
    for (int i = 0; i < vertex_count; i++)
    {
        if (i == prev || i == curr || i == next)
            continue;

        Vec2 p = em->vertices[indices[i]].position;

        // Use barycentric coordinates to check if point is inside triangle
        Vec2 v0v1 = v1 - v0;
        Vec2 v0v2 = v2 - v0;
        Vec2 v0p = p - v0;

        float dot00 = Dot(v0v2, v0v2);
        float dot01 = Dot(v0v2, v0v1);
        float dot02 = Dot(v0v2, v0p);
        float dot11 = Dot(v0v1, v0v1);
        float dot12 = Dot(v0v1, v0p);

        float inv_denom = 1.0f / (dot00 * dot11 - dot01 * dot01);
        float u = (dot11 * dot02 - dot01 * dot12) * inv_denom;
        float v = (dot00 * dot12 - dot01 * dot02) * inv_denom;

        if (u > 0 && v > 0 && u + v < 1)
            return false;
    }

    return true;
}

void TriangulateFace(EditorMesh* em, EditorFace* ef, MeshBuilder* builder)
{
    if (ef->vertex_count < 3)
        return;

    Vec2 uv_color = ColorUV(ef->color.x, ef->color.y);

    // Add all vertices to the builder first
    for (int i = 0; i < ef->vertex_count; i++)
    {
        int vertex_index = em->face_vertices[ef->vertex_offset + i];
        AddVertex(builder, em->vertices[vertex_index].position, ef->normal, uv_color);
    }

    u16 base_vertex = GetVertexCount(builder) - (u16)ef->vertex_count;

    // For triangles, just add directly
    if (ef->vertex_count == 3)
    {
        AddTriangle(builder, base_vertex, base_vertex + 1, base_vertex + 2);
        return;
    }

    // For polygons with more than 3 vertices, use ear clipping
    int indices[MAX_VERTICES];
    for (int i = 0; i < ef->vertex_count; i++)
    {
        indices[i] = em->face_vertices[ef->vertex_offset + i];
    }

    int remaining_vertices = ef->vertex_count;
    int current_index = 0;

    while (remaining_vertices > 3)
    {
        bool found_ear = false;

        for (int attempts = 0; attempts < remaining_vertices; attempts++)
        {
            if (IsEar(em, indices, remaining_vertices, current_index))
            {
                // Found an ear, create triangle
                int prev = (current_index - 1 + remaining_vertices) % remaining_vertices;
                int next = (current_index + 1) % remaining_vertices;

                // Find the corresponding indices in the builder
                u16 tri_indices[3];
                for (u16 i = 0; i < ef->vertex_count; i++)
                {
                    if (em->face_vertices[ef->vertex_offset + i] == indices[prev])
                        tri_indices[0] = base_vertex + i;
                    if (em->face_vertices[ef->vertex_offset + i] == indices[current_index])
                        tri_indices[1] = base_vertex + i;
                    if (em->face_vertices[ef->vertex_offset + i] == indices[next])
                        tri_indices[2] = base_vertex + i;
                }

                AddTriangle(builder, tri_indices[0], tri_indices[1], tri_indices[2]);

                // Remove the ear vertex from the polygon
                for (int i = current_index; i < remaining_vertices - 1; i++)
                {
                    indices[i] = indices[i + 1];
                }
                remaining_vertices--;

                // Adjust current index after removal
                if (current_index >= remaining_vertices)
                    current_index = 0;

                found_ear = true;
                break;
            }

            current_index = (current_index + 1) % remaining_vertices;
        }

        if (!found_ear)
        {
            // Fallback to simple fan triangulation if ear clipping fails
            for (int i = 1; i < remaining_vertices - 1; i++)
            {
                u16 tri_indices[3];
                for (u16 j = 0; j < ef->vertex_count; j++)
                {
                    if (em->face_vertices[ef->vertex_offset + j] == indices[0])
                        tri_indices[0] = base_vertex + j;
                    if (em->face_vertices[ef->vertex_offset + j] == indices[i])
                        tri_indices[1] = base_vertex + j;
                    if (em->face_vertices[ef->vertex_offset + j] == indices[i + 1])
                        tri_indices[2] = base_vertex + j;
                }
                AddTriangle(builder, tri_indices[0], tri_indices[1], tri_indices[2]);
            }
            break;
        }
    }

    // Add the final triangle
    if (remaining_vertices == 3)
    {
        u16 tri_indices[3];
        for (u16 i = 0; i < ef->vertex_count; i++)
        {
            if (em->face_vertices[ef->vertex_offset + i] == indices[0])
                tri_indices[0] = base_vertex + i;
            if (em->face_vertices[ef->vertex_offset + i] == indices[1])
                tri_indices[1] = base_vertex + i;
            if (em->face_vertices[ef->vertex_offset + i] == indices[2])
                tri_indices[2] = base_vertex + i;
        }
        AddTriangle(builder, tri_indices[0], tri_indices[1], tri_indices[2]);
    }
}
