//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

constexpr float OUTLINE_WIDTH = 0.015f;

static void Init(MeshData* m);
extern void InitMeshEditor(MeshData* m);
static void DeleteFaceInternal(MeshData* m, int face_index);
static void RemoveFaceVertices(MeshData* m, int face_index, int remove_at, int remove_count);
static void InsertFaceVertices(MeshData* m, int face_index, int insert_at, int count);
static void MergeFaces(MeshData* m, const EdgeData& shared_edge);
static void DeleteFace(MeshData* m, int face_index);
static void DeleteVertex(MeshData* m, int vertex_index);
static void TriangulateFace(MeshData* m, FaceData* f, MeshBuilder* builder, float depth);

static int GetFaceEdgeIndex(const FaceData& f, const EdgeData& e) {
    for (int vertex_index=0; vertex_index<f.vertex_count; vertex_index++) {
        int v0 = f.vertices[vertex_index];
        int v1 = f.vertices[(vertex_index + 1) % f.vertex_count];
        if (e.v0 == v0 && e.v1 == v1 || e.v0 == v1 && e.v1 == v0)
            return vertex_index;
    }

    return -1;
}

static void DrawMesh(AssetData* a) {
    assert(a->type == ASSET_TYPE_MESH);
    MeshData* m = static_cast<MeshData*>(a);
    DrawMesh(m, Translate(a->position));
}

void DrawMesh(MeshData* m, const Mat3& transform, Material* material) {
    BindMaterial(material ? material : g_view.shaded_material);
    if (g_view.draw_mode == VIEW_DRAW_MODE_WIREFRAME) {
        BindColor(COLOR_EDGE);
        DrawMesh(ToOutlineMesh(m), transform);
    } else {
        BindColor(COLOR_WHITE, GetActivePalette().color_offset_uv);
        DrawMesh(ToMesh(m), transform);
    }

#if 0
    BindColor(COLOR_BLACK);
    BindDepth(GetApplicationTraits()->renderer.max_depth - 0.01f);
    for (int i=0;i<m->tag_count;i++)
        DrawVertex(m->anchors[i].position + m->position);
    BindDepth(0.0f);
#endif
}

Vec2 GetFaceCenter(MeshData* m, FaceData* f) {
    (void)m;
    return f->center;
}

Vec2 GetFaceCenter(MeshData* m, int face_index) {
    return m->faces[face_index].center;
}

bool IsVertexOnOutsideEdge(MeshData* m, int v0) {
    for (int i = 0; i < m->edge_count; i++) {
        EdgeData& ee = m->edges[i];
        if (ee.face_count == 1 && (ee.v0 == v0 || ee.v1 == v0))
            return true;
    }

    return false;
}

static int GetEdge(MeshData* m, int v0, int v1) {
    int fv0 = Min(v0, v1);
    int fv1 = Max(v0, v1);
    for (int i = 0; i < m->edge_count; i++) {
        EdgeData& ee = m->edges[i];
        if (ee.v0 == fv0 && ee.v1 == fv1)
            return i;
    }

    return -1;
}

int GetOrAddEdge(MeshData* m, int v0, int v1, int face_index) {
    int fv0 = Min(v0, v1);
    int fv1 = Max(v0, v1);

    for (int i = 0; i < m->edge_count; i++) {
        EdgeData& ee = m->edges[i];
        if (ee.v0 == fv0 && ee.v1 == fv1) {
            if (ee.face_index[0] > face_index) {
                int temp = ee.face_index[0];
                ee.face_index[0] = face_index;
                ee.face_index[1] = temp;
            } else {
                ee.face_index[ee.face_count] = face_index;
            }

            ee.face_count++;
            return i;
        }
    }

    // Not found - add it
    if (m->edge_count >= MAX_EDGES)
        return -1;

    int edge_index = m->edge_count++;
    EdgeData& ee = m->edges[edge_index];
    ee.face_count = 1;
    ee.face_index[0] = face_index;
    ee.v0 = fv0;
    ee.v1 = fv1;
    ee.normal = Normalize(-Perpendicular(m->vertices[v1].position - m->vertices[v0].position));

    return edge_index;
}

// Compute centroid using signed area formula (works for concave polygons and holes)
static Vec2 ComputeFaceCentroid(MeshData* m, FaceData& f) {
    if (f.vertex_count < 3)
        return VEC2_ZERO;

    float signed_area = 0.0f;
    Vec2 centroid = VEC2_ZERO;

    for (int i = 0; i < f.vertex_count; i++) {
        Vec2 p0 = m->vertices[f.vertices[i]].position;
        Vec2 p1 = m->vertices[f.vertices[(i + 1) % f.vertex_count]].position;
        float cross = p0.x * p1.y - p1.x * p0.y;
        signed_area += cross;
        centroid.x += (p0.x + p1.x) * cross;
        centroid.y += (p0.y + p1.y) * cross;
    }

    signed_area *= 0.5f;

    // Handle degenerate faces (zero area)
    if (Abs(signed_area) < F32_EPSILON) {
        // Fall back to simple average
        centroid = VEC2_ZERO;
        for (int i = 0; i < f.vertex_count; i++)
            centroid += m->vertices[f.vertices[i]].position;
        return centroid / (float)f.vertex_count;
    }

    float factor = 1.0f / (6.0f * signed_area);
    return centroid * factor;
}

void UpdateEdges(MeshData* m) {
    m->edge_count = 0;

    for (int vertex_index=0; vertex_index < m->vertex_count; vertex_index++) {
        m->vertices[vertex_index].edge_normal = VEC2_ZERO;
        m->vertices[vertex_index].ref_count = 0;
    }

    for (int face_index=0; face_index < m->face_count; face_index++) {
        FaceData& f = m->faces[face_index];

        // Compute and cache face centroid
        f.center = ComputeFaceCentroid(m, f);

        for (int vertex_index = 0; vertex_index<f.vertex_count - 1; vertex_index++){
            int v0 = f.vertices[vertex_index];
            int v1 = f.vertices[vertex_index + 1];
            GetOrAddEdge(m, v0, v1, face_index);
        }

        int vs = f.vertices[f.vertex_count - 1];
        int ve = f.vertices[0];
        GetOrAddEdge(m, vs, ve, face_index);
    }

    for (int edge_index=0; edge_index<m->edge_count; edge_index++) {
        EdgeData& e = m->edges[edge_index];
        m->vertices[e.v0].ref_count++;
        m->vertices[e.v1].ref_count++;
        if (e.face_count == 1) {
            m->vertices[e.v0].edge_normal += e.normal;
            m->vertices[e.v1].edge_normal += e.normal;
        }
    }

    for (int vertex_index=0; vertex_index<m->vertex_count; vertex_index++) {
        VertexData& v = m->vertices[vertex_index];
        if (Length(v.edge_normal) > F32_EPSILON)
            v.edge_normal = Normalize(v.edge_normal);
    }
}

void MarkDirty(MeshData* m) {
    Free(m->mesh);
    Free(m->outline);
    m->mesh = nullptr;
    m->outline = nullptr;
}

Mesh* ToMesh(MeshData* m, bool upload, bool use_cache) {
    if (use_cache && m->mesh)
        return m->mesh;

    PushScratch();
    MeshBuilder* builder = CreateMeshBuilder(ALLOCATOR_SCRATCH, MAX_VERTICES, MAX_INDICES);

    float depth = 0.01f + 0.99f * (m->depth - MIN_DEPTH) / (float)(MAX_DEPTH-MIN_DEPTH);
    for (int i = 0; i < m->face_count; i++)
        TriangulateFace(m, m->faces + i, builder, depth);

    Mesh* mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE, upload);
    m->bounds = mesh ? GetBounds(mesh) : BOUNDS2_ZERO;

    if (use_cache)
        m->mesh = mesh;

    PopScratch();

    return mesh;
}

static void AddVertexWeights(MeshBuilder* builder, const VertexData& v) {
    for (int weight_index=0; weight_index < MESH_MAX_VERTEX_WEIGHTS; ++weight_index)
        if (v.weights[weight_index].weight > F32_EPSILON)
            AddVertexWeight(builder, v.weights[weight_index].bone_index, v.weights[weight_index].weight);
}

Mesh* ToOutlineMesh(MeshData* m) {
    if (m->outline && m->outline_version == g_view.zoom_version)
        return m->outline;

    PushScratch();
    MeshBuilder* builder = CreateMeshBuilder(ALLOCATOR_SCRATCH, MAX_VERTICES, MAX_INDICES);

    float outline_size = g_view.zoom_ref_scale * OUTLINE_WIDTH * 0.5f;

    for (int i=0; i < m->edge_count; i++) {
        const EdgeData& ee = m->edges[i];
        if (ee.face_count > 1)
            continue;

        const VertexData& v0 = m->vertices[ee.v0];
        const VertexData& v1 = m->vertices[ee.v1];
        Vec2 p0 = {v0.position.x, v0.position.y};
        Vec2 p1 = {v1.position.x, v1.position.y};
        Vec2 n = Perpendicular(Normalize(p1 - p0));
        u16 base = GetVertexCount(builder);
        AddVertex(builder, p0 - n * outline_size);
        AddVertexWeights(builder, v0);
        AddVertex(builder, p0 + n * outline_size);
        AddVertexWeights(builder, v0);
        AddVertex(builder, p1 + n * outline_size);
        AddVertexWeights(builder, v1);
        AddVertex(builder, p1 - n * outline_size);
        AddVertexWeights(builder, v1);
        AddTriangle(builder, base+0, base+1, base+3);
        AddTriangle(builder, base+1, base+2, base+3);
    }

    m->outline = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE, true);
    m->outline_version = g_view.zoom_version;

    Free(builder);
    PopScratch();

    return m->outline;
}

void SetEdgeColor(MeshData* m, const Vec2Int& color) {
    m->edge_color = color;
    MarkDirty(m);
}

void SetSelectedTrianglesColor(MeshData* m, const Vec2Int& color) {
    int count = 0;
    for (i32 i = 0; i < m->face_count; i++) {
        FaceData& et = m->faces[i];
        if (et.selected) {
            et.color = color;
            count++;
        }
    }

    if (!count)
        return;

    MarkDirty(m);
}

static int CountSharedEdges(MeshData* m, int face_index0, int face_index1) {
    assert(face_index0 < face_index1);

    int shared_edge_count = 0;
    for (int edge_index=0; edge_index<m->edge_count; edge_index++) {
        EdgeData& ee = m->edges[edge_index];
        if (ee.face_count != 2)
            continue;

        if (ee.face_index[0] == face_index0 && ee.face_index[1] == face_index1)
            shared_edge_count++;
    }

    return shared_edge_count;
}

static void CollapseEdge(MeshData* m, int edge_index) {
    assert(m);
    assert(edge_index >= 0 && edge_index < m->edge_count);

    EdgeData& e = m->edges[edge_index];
    VertexData& v0 = m->vertices[e.v0];
    VertexData& v1 = m->vertices[e.v1];

    DeleteVertex(m, v0.ref_count > v1.ref_count ? e.v1 : e.v0);
    UpdateEdges(m);
    MarkDirty(m);
}

void DissolveEdge(MeshData* m, int edge_index) {
    EdgeData& ee = m->edges[edge_index];
    assert(ee.face_count > 0);

    if (ee.face_count == 1)
    {
        FaceData& ef = m->faces[ee.face_index[0]];
        if (ef.vertex_count <= 3)
        {
            DeleteFace(m, ee.face_index[0]);
            return;
        }

        CollapseEdge(m, edge_index);
        return;
    }

    // Slit edge: same face on both sides - cannot dissolve
    if (ee.face_index[0] == ee.face_index[1])
        return;

    int shared_edge_count = CountSharedEdges (m, ee.face_index[0], ee.face_index[1]);
    if (shared_edge_count == 1)
    {
        MergeFaces(m, ee);
        return;
    }

    CollapseEdge(m, edge_index);
}

static void DeleteVertex(MeshData* m, int vertex_index) {
    assert(vertex_index >= 0 && vertex_index < m->vertex_count);

    for (int face_index=m->face_count-1; face_index >= 0; face_index--) {
        FaceData& f = m->faces[face_index];
        int vertex_pos = -1;
        for (int face_vertex_index=0; face_vertex_index<f.vertex_count; face_vertex_index++) {
            if (f.vertices[face_vertex_index] == vertex_index) {
                vertex_pos = face_vertex_index;
                break;
            }
        }

        if (vertex_pos == -1)
            continue;

        if (f.vertex_count <= 3)
            DeleteFaceInternal(m, face_index);
        else
            RemoveFaceVertices(m, face_index, vertex_pos, 1);
    }

    for (int face_index=0; face_index < m->face_count; face_index++) {
        FaceData& f = m->faces[face_index];
        for (int face_vertex_index=0; face_vertex_index<f.vertex_count; face_vertex_index++) {
            int v_idx = f.vertices[face_vertex_index];
            if (v_idx > vertex_index)
                f.vertices[face_vertex_index] = v_idx - 1;
        }
    }

    for (; vertex_index < m->vertex_count - 1; vertex_index++)
        m->vertices[vertex_index] = m->vertices[vertex_index + 1];

    m->vertex_count--;

    UpdateEdges(m);
}

static void DeleteFaceInternal(MeshData* m, int face_index) {
    assert(face_index >= 0 && face_index < m->face_count);
    RemoveFaceVertices(m, face_index, 0, -1);
    for (int i=face_index; i < m->face_count - 1; i++)
        m->faces[i] = m->faces[i + 1];
    m->face_count--;
}

static void DeleteFace(MeshData* m, int face_index) {
    DeleteFaceInternal(m, face_index);
    UpdateEdges(m);
    MarkDirty(m);
}

void DissolveSelectedFaces(MeshData* m) {
    for (int face_index=m->face_count - 1; face_index>=0; face_index--)
    {
        FaceData& ef = m->faces[face_index];
        if (!ef.selected)
            continue;

        DeleteFace(m, face_index);
    }
}

static void MergeFaces(MeshData* m, const EdgeData& shared_edge) {
    assert(shared_edge.face_count == 2);
    assert(CountSharedEdges(m, shared_edge.face_index[0], shared_edge.face_index[1]) == 1);

    FaceData& face0 = m->faces[shared_edge.face_index[0]];
    FaceData& face1 = m->faces[shared_edge.face_index[1]];

    int edge_pos0 = GetFaceEdgeIndex(face0, shared_edge);
    int edge_pos1 = GetFaceEdgeIndex(face1, shared_edge);
    assert(edge_pos0 != -1);
    assert(edge_pos1 != -1);

    int insert_pos = (edge_pos0 + 1) % face0.vertex_count;
    InsertFaceVertices(m, shared_edge.face_index[0], insert_pos, face1.vertex_count - 2);

    for (int face_index=0; face_index<face1.vertex_count - 2; face_index++)
        face0.vertices[insert_pos + face_index] =
            face1.vertices[((edge_pos1 + 2 + face_index) % face1.vertex_count)];

    DeleteFaceInternal(m, shared_edge.face_index[1]);
    UpdateEdges(m);
    MarkDirty(m);
}

void DissolveSelectedVertices(MeshData* m) {
    int vertices[MAX_VERTICES];
    int vertex_count = GetSelectedVertices(m, vertices);
    for (int vertex_index=vertex_count-1; vertex_index>=0; vertex_index--)
        DeleteVertex(m, vertices[vertex_index]);

    MarkDirty(m);
}

static void InsertFaceVertices(MeshData* m, int face_index, int insert_at, int count) {
    FaceData& f = m->faces[face_index];

    for (int vertex_index=f.vertex_count + count; vertex_index > insert_at; vertex_index--)
        f.vertices[vertex_index] = f.vertices[vertex_index-count];

    for (int i=0; i<count; i++)
        f.vertices[insert_at + i] = -1;

    f.vertex_count += count;
}

static void RemoveFaceVertices(MeshData* m, int face_index, int remove_at, int remove_count) {
    FaceData& f = m->faces[face_index];
    if (remove_count == -1)
        remove_count = f.vertex_count - remove_at;

    assert(remove_at >= 0 && remove_at + remove_count <= f.vertex_count);

    for (int vertex_index=remove_at; vertex_index + remove_count < f.vertex_count; vertex_index++)
        f.vertices[vertex_index] = f.vertices[vertex_index + remove_count];

    f.vertex_count -= remove_count;
}

int CreateFace(MeshData* m) {
    int selected_vertices[MAX_VERTICES];
    int selected_count = GetSelectedVertices(m, selected_vertices);
    if (selected_count < 3)
        return -1;

    if (m->face_count >= MAX_FACES)
        return -1;

    for (int i = 0; i < selected_count; i++) {
        int v0 = selected_vertices[i];
        int v1 = selected_vertices[(i + 1) % selected_count];

        int edge_index = GetEdge(m, v0, v1);
        if (edge_index != -1) {
            const EdgeData& e = m->edges[edge_index];
            if (e.face_count >= 2)
                return -1;
        }
    }

    // Find color
    int color_counts[COLOR_COUNT] = {};
    for (int i = 0; i < selected_count; i++) {
        int v0 = selected_vertices[i];
        int v1 = selected_vertices[(i + 1) % selected_count];
        int edge_index = GetEdge(m, v0, v1);
        if (edge_index != -1) {
            const EdgeData& e = m->edges[edge_index];
            for (int face_idx = 0; face_idx < e.face_count; face_idx++) {
                int color_x = m->faces[e.face_index[face_idx]].color.x;
                color_counts[color_x]++;
            }
        }
    }

    Vec2Int best_color = {1, 0};
    int best_count = 0;
    for (int i = 0; i < 64; i++) {
        if (color_counts[i] > best_count) {
            best_count = color_counts[i];
            best_color = {i, 0};
        }
    }

    Vec2 centroid = VEC2_ZERO;
    for (int i = 0; i < selected_count; i++)
        centroid += m->vertices[selected_vertices[i]].position;
    centroid = centroid / (float)selected_count;

    struct VertexAngle {
        int vertex_index;
        float angle;
    };

    VertexAngle vertex_angles[MAX_VERTICES];
    for (int i = 0; i < selected_count; i++) {
        Vec2 dir = m->vertices[selected_vertices[i]].position - centroid;
        vertex_angles[i].vertex_index = selected_vertices[i];
        vertex_angles[i].angle = atan2f(dir.y, dir.x);
    }

    for (int i = 0; i < selected_count - 1; i++) {
        for (int j = i + 1; j < selected_count; j++) {
            if (vertex_angles[i].angle > vertex_angles[j].angle) {
                VertexAngle temp = vertex_angles[i];
                vertex_angles[i] = vertex_angles[j];
                vertex_angles[j] = temp;
            }
        }
    }

    int face_index = m->face_count++;
    FaceData& f = m->faces[face_index];
    f.vertex_count = selected_count;
    f.color = best_color;
    f.normal = {0, 0, 1};
    f.selected = false;

    for (int i = 0; i < selected_count; i++)
        f.vertices[i] = vertex_angles[i].vertex_index;

    UpdateEdges(m);
    MarkDirty(m);

    return face_index;
}

int SplitFaces(MeshData* m, int v0, int v1) {
    if (m->face_count >= MAX_FACES)
        return -1;

    if (GetEdge(m, v0, v1) != -1)
        return -1;

    int face_index = 0;
    int v0_pos = -1;
    int v1_pos = -1;
    for (; face_index < m->face_count; face_index++) {
        FaceData& f = m->faces[face_index];

        v0_pos = -1;
        v1_pos = -1;
        for (int i = 0; i < f.vertex_count && (v0_pos == -1 || v1_pos == -1); i++) {
            int vertex_index = f.vertices[i];
            if (vertex_index == v0) v0_pos = i;
            if (vertex_index == v1) v1_pos = i;
        }

        if (v0_pos != -1 && v1_pos != -1)
            break;
    }

    if (face_index >= m->face_count)
        return -1;

    if (v0_pos > v1_pos)
    {
        int temp = v0_pos;
        v0_pos = v1_pos;
        v1_pos = temp;
    }

    FaceData& old_face = m->faces[face_index];
    FaceData& new_face = m->faces[m->face_count++];
    new_face.color = old_face.color;
    new_face.normal = old_face.normal;
    new_face.selected = old_face.selected;

    int old_vertex_count = old_face.vertex_count - (v1_pos - v0_pos - 1);
    int new_vertex_count = v1_pos - v0_pos + 1;

    new_face.vertex_count = new_vertex_count;
    for (int vertex_index=0; vertex_index<new_vertex_count; vertex_index++)
        new_face.vertices[vertex_index] = old_face.vertices[v0_pos + vertex_index];

    for (int vertex_index=0; v1_pos+vertex_index<old_face.vertex_count; vertex_index++)
        old_face.vertices[v0_pos + vertex_index + 1] =
            old_face.vertices[v1_pos + vertex_index];

    RemoveFaceVertices(m, face_index, old_vertex_count, old_face.vertex_count - old_vertex_count);

    UpdateEdges(m);
    MarkDirty(m);

    return GetEdge(m, old_face.vertices[v0_pos], old_face.vertices[(v0_pos + 1) % old_face.vertex_count]);
}

int SplitEdge(MeshData* m, int edge_index, float edge_pos, bool update) {
    assert(edge_index >= 0 && edge_index < m->edge_count);

    if (m->vertex_count >= MAX_VERTICES)
        return -1;

    if (m->edge_count >= MAX_VERTICES)
        return -1;

    EdgeData& e = m->edges[edge_index];
    VertexData& v0 = m->vertices[e.v0];
    VertexData& v1 = m->vertices[e.v1];

    int new_vertex_index = m->vertex_count++;
    VertexData& new_vertex = m->vertices[new_vertex_index];
    new_vertex.edge_size = (v0.edge_size + v1.edge_size) * 0.5f;
    new_vertex.position = (v0.position * (1.0f - edge_pos) + v1.position * edge_pos);

    int face_count = m->face_count;
    for (int face_index = 0; face_index < face_count; face_index++) {
        FaceData& f = m->faces[face_index];

        int face_edge = GetFaceEdgeIndex(f, e);
        if (face_edge == -1)
            continue;

        InsertFaceVertices(m, face_index, face_edge + 1, 1);
        f.vertices[face_edge + 1] = new_vertex_index;
    }

    if (update) {
        UpdateEdges(m);
        MarkDirty(m);
    }

    return new_vertex_index;
}

int HitTestVertex(const Vec2& position, const Vec2& hit_pos, float size_mult) {
    float size = g_view.select_size * size_mult;
    float dist = Length(hit_pos - position);
    return dist <= size;
}

int HitTestVertex(MeshData* m, const Mat3& transform, const Vec2& position, float size_mult) {
    float size = g_view.select_size * size_mult;
    float best_dist = F32_MAX;
    int best_vertex = -1;
    for (int i = 0; i < m->vertex_count; i++) {
        const VertexData& v = m->vertices[i];
        float dist = Length(position - TransformPoint(transform, v.position));
        if (dist <= size && dist < best_dist) {
            best_vertex = i;
            best_dist = dist;
        }
    }

    return best_vertex;
}

int HitTestEdge(MeshData* m, const Mat3& transform, const Vec2& hit_pos, float* where, float size_mult) {
    const float size = g_view.select_size * 0.75f * size_mult;
    float best_dist = F32_MAX;
    int best_edge = -1;
    float best_where = 0.0f;
    for (int i = 0; i < m->edge_count; i++) {
        const EdgeData& e = m->edges[i];
        Vec2 v0 = TransformPoint(transform, m->vertices[e.v0].position);
        Vec2 v1 = TransformPoint(transform, m->vertices[e.v1].position);
        Vec2 edge_dir = Normalize(v1 - v0);
        Vec2 to_mouse = hit_pos - v0;
        float edge_length = Length(v1 - v0);
        float proj = Dot(to_mouse, edge_dir);
        if (proj >= 0 && proj <= edge_length) {
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

void Center(MeshData* m) {
    if (m->vertex_count == 0)
        return;

    RecordUndo(m);

    Bounds2 bounds = {m->vertices[0].position, m->vertices[0].position};
    for (int vertex_index=1; vertex_index<m->vertex_count; vertex_index++)
        bounds = Union(bounds, m->vertices[vertex_index].position);

    Vec2 size = GetSize(bounds);
    Vec2 offset = bounds.min + size * 0.5f;
    for (int i=0; i<m->vertex_count; i++)
        m->vertices[i].position = m->vertices[i].position - offset;

    UpdateEdges(m);
    MarkDirty(m);
    MarkModified();
}

void SwapFace(MeshData* m, int face_index_a, int face_index_b) {
    FaceData temp = m->faces[face_index_a];
    m->faces[face_index_a] = m->faces[face_index_b];
    m->faces[face_index_b] = temp;
}

bool OverlapBounds(MeshData* m, const Vec2& position, const Bounds2& hit_bounds) {
    return Intersects(m->bounds + position, hit_bounds);
}

int HitTestFaces(MeshData* m, const Mat3& transform, const Vec2& position, int* faces, int max_faces) {
    int hit_count = 0;
    for (int i = m->face_count - 1; i >= 0 && hit_count < max_faces; i--) {
        FaceData& f = m->faces[i];

        // Ray casting algorithm - works for both convex and concave polygons
        int intersections = 0;

        for (int vertex_index = 0; vertex_index < f.vertex_count; vertex_index++) {
            int v0_idx = f.vertices[vertex_index];
            int v1_idx = f.vertices[(vertex_index + 1) % f.vertex_count];

            Vec2 v0 = TransformPoint(transform, m->vertices[v0_idx].position);
            Vec2 v1 = TransformPoint(transform, m->vertices[v1_idx].position);

            // Cast horizontal ray to the right from hit_pos
            // Check if this edge intersects the ray
            float min_y = Min(v0.y, v1.y);
            float max_y = Max(v0.y, v1.y);

            // Skip horizontal edges and edges that don't cross the ray's Y level
            if (position.y < min_y || position.y >= max_y || min_y == max_y)
                continue;

            // Calculate X intersection point
            float t = (position.y - v0.y) / (v1.y - v0.y);
            float x_intersect = v0.x + t * (v1.x - v0.x);

            // Count intersection if it's to the right of the point
            if (x_intersect > position.x)
                intersections++;
        }

        // Point is inside if odd number of intersections
        if (!(intersections % 2) == 1)
            continue;

        faces[hit_count++] = i;
    }

    return hit_count;
}

int HitTestFace(MeshData* m, const Mat3& transform, const Vec2& position) {
    int faces[1];
    int hit_count = HitTestFaces(m, transform, position, faces, 1);
    return hit_count > 0 ? faces[0] : -1;
}

static void ParseVertexEdge(VertexData& ev, Tokenizer& tk) {
    if (!ExpectFloat(tk, &ev.edge_size))
        ThrowError("missing vertex edge value");
}

static void ParseVertexWeight(Tokenizer& tk, VertexWeight& vertex_weight) {
    f32 weight = 0.0f;
    i32 index = 0;
    if (!ExpectInt(tk, &index))
        ThrowError("missing weight bone index");

    if (!ExpectFloat(tk, &weight))
        ThrowError("missing vertex weight value");

    vertex_weight = { index, weight };
}

static void ParseTag(MeshData* m, Tokenizer& tk) {
    if (!ExpectQuotedString(tk))
        ThrowError("missing tag name");

    TagData tag = {};
    tag.name = GetName(tk);

    int weight_count = 0;
    while (!IsEOF(tk)) {
        if (!ExpectIdentifier(tk, "p")) {
            tag.position.x = ExpectFloat(tk);
            tag.position.y = ExpectFloat(tk);
        }
        ThrowError("missing anchor x coordinate");
        if (ExpectIdentifier(tk, "r")) {
            tag.rotation = ExpectFloat(tk);
        } else if (ExpectIdentifier(tk, "w")) {
            ParseVertexWeight(tk, tag.weights[weight_count++]);
        } else {
            break;
        }
    }

    m->tags[m->tag_count++] = tag;
}

static void ParseVertex(MeshData* m, Tokenizer& tk) {
    if (m->vertex_count >= MAX_VERTICES)
        ThrowError("too many vertices");

    f32 x;
    if (!ExpectFloat(tk, &x))
        ThrowError("missing vertex x coordinate");

    f32 y;
    if (!ExpectFloat(tk, &y))
        ThrowError("missing vertex y coordinate");

    VertexData& v = m->vertices[m->vertex_count++];
    v.position = {x,y};

    int weight_count = 0;
    while (!IsEOF(tk)) {
        if (ExpectIdentifier(tk, "e")) {
            ParseVertexEdge(v, tk);
        } else if (ExpectIdentifier(tk, "h")) {
            float temp = 0.0f;
            ExpectFloat(tk, &temp);
        } else if (ExpectIdentifier(tk, "w")) {
            ParseVertexWeight(tk, v.weights[weight_count++]);
        } else {
            break;
        }
    }
}

static void ParseEdgeColor(MeshData* em, Tokenizer& tk) {
    int cx;
    if (!ExpectInt(tk, &cx))
        ThrowError("missing edge color x value");

    int cy;
    if (!ExpectInt(tk, &cy))
        ThrowError("missing edge color y value");

    em->edge_color = {(u8)cx, (u8)cy};
}

static void ParseFaceColor(FaceData& ef, Tokenizer& tk) {
    int cx;
    if (!ExpectInt(tk, &cx))
        ThrowError("missing face color x value");

    int cy;
    if (!ExpectInt(tk, &cy))
        ThrowError("missing face color y value");

    ef.color = {(u8)cx, (u8)cy};
}

static void ParseFaceNormal(FaceData& ef, Tokenizer& tk) {
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

static void ParseFace(MeshData* m, Tokenizer& tk) {
    if (m->face_count >= MAX_FACES)
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

    FaceData& f = m->faces[m->face_count++];
    f.vertices[f.vertex_count++] = v0;
    f.vertices[f.vertex_count++] = v1;
    f.vertices[f.vertex_count++] = v2;

    while (ExpectInt(tk, &v2))
        f.vertices[f.vertex_count++] = v2;

    // Handle a degenerate case where there are two points in a row.
    if (f.vertices[f.vertex_count-1] == f.vertices[0])
        f.vertex_count--;

    if (v0 < 0 || v0 >= m->vertex_count || v1 < 0 || v1 >= m->vertex_count || v2 < 0 || v2 >= m->vertex_count)
        ThrowError("face vertex index out of range");

    f.color = {0, 0};

    while (!IsEOF(tk))
    {
        if (ExpectIdentifier(tk, "c"))
            ParseFaceColor(f, tk);
        else if (ExpectIdentifier(tk, "n"))
            ParseFaceNormal(f, tk);
        else
            break;
    }
}

static void ParseDepth(MeshData* m, Tokenizer& tk) {
    float depth = 0.0f;
    if (!ExpectFloat(tk, &depth))
        ThrowError("missing mesh depth value");

    m->depth = (int)(depth * (MAX_DEPTH - MIN_DEPTH) + MIN_DEPTH);
}

void LoadMeshData(MeshData* m, Tokenizer& tk, bool multiple_mesh=false) {
    while (!IsEOF(tk)) {
        if (ExpectIdentifier(tk, "v")) {
            ParseVertex(m, tk);
        } else if (ExpectIdentifier(tk, "t")) {
            ParseTag(m, tk);
        } else if (ExpectIdentifier(tk, "d")) {
            ParseDepth(m, tk);
        } else if (ExpectIdentifier(tk, "f")) {
            ParseFace(m, tk);
        } else if (ExpectIdentifier(tk, "e")) {
            ParseEdgeColor(m, tk);
        } else if (multiple_mesh && Peek(tk, "m")) {
            break;
        } else {
            char error[1024];
            GetString(tk, error, sizeof(error) - 1);
            ThrowError("invalid token '%s' in mesh", error);
        }
    }

    UpdateEdges(m);
    MarkDirty(m);
    ToMesh(m, false);
}

void SerializeMesh(Mesh* m, Stream* stream) {
    if (!m) {
        WriteStruct(stream, BOUNDS2_ZERO);
        WriteU16(stream, 0);
        WriteU16(stream, 0);
        return;
    }

    WriteStruct(stream, GetBounds(m));
    WriteU16(stream, GetVertexCount(m));
    WriteU16(stream, GetIndexCount(m));

    const MeshVertex* v = GetVertices(m);
    WriteBytes(stream, v, sizeof(MeshVertex) * GetVertexCount(m));

    const u16* i = GetIndices(m);
    WriteBytes(stream, i, sizeof(u16) * GetIndexCount(m));
}

static void LoadMeshData(AssetData* a) {
    assert(a);
    assert(a->type == ASSET_TYPE_MESH);
    MeshData* m = (MeshData*)a;

    std::string contents = ReadAllText(ALLOCATOR_DEFAULT, a->path);
    Tokenizer tk;
    Init(tk, contents.c_str());
    LoadMeshData(m, tk);
}

MeshData* LoadMeshData(const std::filesystem::path& path) {
    std::string contents = ReadAllText(ALLOCATOR_DEFAULT, path);
    Tokenizer tk;
    Init(tk, contents.c_str());

    MeshData* m = static_cast<MeshData*>(CreateAssetData(path));
    assert(m);
    Init(m);
    LoadMeshData(m);
    return m;
}

static void WriteVertexWeights(Stream* stream, const VertexWeight* weights) {
    for (int weight_index=0; weight_index<MESH_MAX_VERTEX_WEIGHTS; weight_index++) {
        const VertexWeight& w = weights[weight_index];
        if (w.weight <= 0.0f)
            continue;

        WriteCSTR(stream, " w %d %f", w.bone_index, w.weight);
    }
}

void SaveMeshData(MeshData* m, Stream* stream) {
    WriteCSTR(stream, "d %f\n", (m->depth - MIN_DEPTH) / (float)(MAX_DEPTH - MIN_DEPTH));
    WriteCSTR(stream, "e %d %d\n", m->edge_color.x, m->edge_color.y);
    WriteCSTR(stream, "\n");

    for (int tag_index=0; tag_index<m->tag_count; tag_index++) {
        const TagData& t = m->tags[tag_index];
        WriteCSTR(stream, "t %s p %f %f r %f", t.name->value, t.position.x, t.position.y, t.rotation);
        WriteVertexWeights(stream, t.weights);
        WriteCSTR(stream, "\n");
    }

    for (int i=0; i<m->vertex_count; i++) {
        const VertexData& v = m->vertices[i];
        WriteCSTR(stream, "v %f %f e %f", v.position.x, v.position.y, v.edge_size);
        WriteVertexWeights(stream, v.weights);
        WriteCSTR(stream, "\n");
    }

    WriteCSTR(stream, "\n");

    for (int i=0; i<m->face_count; i++) {
        const FaceData& f = m->faces[i];

        WriteCSTR(stream, "f ");
        for (int vertex_index=0; vertex_index<f.vertex_count; vertex_index++)
            WriteCSTR(stream, " %d", f.vertices[vertex_index]);

        WriteCSTR(stream, " c %d %d n %f %f %f\n", f.color.x, f.color.y, f.normal.x, f.normal.y, f.normal.z);
    }
}

static void SaveMeshData(AssetData* a, const std::filesystem::path& path) {
    assert(a->type == ASSET_TYPE_MESH);
    MeshData* m = static_cast<MeshData*>(a);
    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);
    SaveMeshData(m, stream);
    SaveStream(stream, path);
    Free(stream);
}

AssetData* NewMeshData(const std::filesystem::path& path) {
    constexpr const char* default_mesh = "v -1 -1 e {0} h 0\n"
                               "v 1 -1 e {0} h 0\n"
                               "v 1 1 e {0} h 0\n"
                               "v -1 1 e {0} h 0\n"
                               "\n"
                               "f 0 1 2 3 c 0 0\n";

    float edge_size = g_config->GetFloat("mesh", "default_edge_size", 1.0f);

    std::string text = std::format(default_mesh, edge_size);

    if (g_view.selected_asset_count == 1) {
        AssetData* selected = GetFirstSelectedAsset();
        assert(selected);
        if (selected->type == ASSET_TYPE_MESH)
            text = ReadAllText(ALLOCATOR_DEFAULT, selected->path);
    }

    std::filesystem::path full_path = path.is_relative() ?  std::filesystem::current_path() / "assets" / "meshes" / path : path;
    full_path += ".mesh";

    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);
    WriteCSTR(stream, text.c_str());
    SaveStream(stream, full_path);
    Free(stream);

    return LoadMeshData(full_path);
}

static void AllocateData(MeshData* m) {
    m->data = static_cast<MeshRuntimeData*>(Alloc(ALLOCATOR_DEFAULT, sizeof(MeshRuntimeData)));
    m->vertices = m->data->vertices;
    m->edges = m->data->edges;
    m->faces = m->data->faces;
    m->tags = m->data->tags;
}

static void CloneMeshData(AssetData* a) {
    assert(a->type == ASSET_TYPE_MESH);
    MeshData* m = static_cast<MeshData*>(a);
    m->mesh = nullptr;
    m->outline = nullptr;

    MeshRuntimeData* old_data = m->data;
    AllocateData(m);
    memcpy(m->data, old_data, sizeof(MeshRuntimeData));
}

void InitMeshData(AssetData* a) {
    assert(a);
    assert(a->type == ASSET_TYPE_MESH);
    MeshData* em = static_cast<MeshData*>(a);
    Init(em);
}

static bool IsEar(MeshData* m, int* indices, int vertex_count, int ear_index) {
    int prev = (ear_index - 1 + vertex_count) % vertex_count;
    int curr = ear_index;
    int next = (ear_index + 1) % vertex_count;

    Vec2 v0 = m->vertices[indices[prev]].position;
    Vec2 v1 = m->vertices[indices[curr]].position;
    Vec2 v2 = m->vertices[indices[next]].position;

    // Check if triangle has correct winding (counter-clockwise)
    float cross = (v1.x - v0.x) * (v2.y - v0.y) - (v2.x - v0.x) * (v1.y - v0.y);
    if (cross <= 0)
        return false;

    // Check if any other vertex is inside this triangle
    for (int i = 0; i < vertex_count; i++)
    {
        if (i == prev || i == curr || i == next)
            continue;

        Vec2 p = m->vertices[indices[i]].position;

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

static void TriangulateFace(MeshData* m, FaceData* f, MeshBuilder* builder, float depth) {
    if (f->vertex_count < 3)
        return;

    Vec2 uv_color = ColorUV(f->color.x, f->color.y);

    for (int vertex_index = 0; vertex_index < f->vertex_count; vertex_index++) {
        VertexData& v = m->vertices[f->vertices[vertex_index]];
        MeshVertex mv = { .position = v.position, .depth = depth, .uv = uv_color };
        mv.bone_weights.x = v.weights[0].weight;
        mv.bone_weights.y = v.weights[1].weight;
        mv.bone_weights.z = v.weights[2].weight;
        mv.bone_weights.w = v.weights[3].weight;
        mv.bone_indices.x = v.weights[0].bone_index;
        mv.bone_indices.y = v.weights[1].bone_index;
        mv.bone_indices.z = v.weights[2].bone_index;
        mv.bone_indices.w = v.weights[3].bone_index;
        AddVertex(builder, mv);
    }

    u16 base_vertex = GetVertexCount(builder) - (u16)f->vertex_count;
    if (f->vertex_count == 3) {
        AddTriangle(builder, base_vertex, base_vertex + 1, base_vertex + 2);
        return;
    }

    int indices[MAX_VERTICES];
    for (int vertex_index = 0; vertex_index < f->vertex_count; vertex_index++)
        indices[vertex_index] = f->vertices[vertex_index];

    int remaining_vertices = f->vertex_count;
    int current_index = 0;

    while (remaining_vertices > 3) {
        bool found_ear = false;

        for (int attempts = 0; attempts < remaining_vertices; attempts++) {
            if (IsEar(m, indices, remaining_vertices, current_index)) {
                // Found an ear, create triangle
                int prev = (current_index - 1 + remaining_vertices) % remaining_vertices;
                int next = (current_index + 1) % remaining_vertices;

                // Find the corresponding indices in the builder
                u16 tri_indices[3];
                for (u16 vertex_index = 0; vertex_index < f->vertex_count; vertex_index++) {
                    if (f->vertices[vertex_index] == indices[prev])
                        tri_indices[0] = base_vertex + vertex_index;
                    if (f->vertices[vertex_index] == indices[current_index])
                        tri_indices[1] = base_vertex + vertex_index;
                    if (f->vertices[vertex_index] == indices[next])
                        tri_indices[2] = base_vertex + vertex_index;
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

        if (!found_ear) {
            for (int i = 1; i < remaining_vertices - 1; i++) {
                u16 tri_indices[3];
                for (u16 j = 0; j < f->vertex_count; j++) {
                    if (f->vertices[j] == indices[0])
                        tri_indices[0] = base_vertex + j;
                    if (f->vertices[j] == indices[i])
                        tri_indices[1] = base_vertex + j;
                    if (f->vertices[j] == indices[i + 1])
                        tri_indices[2] = base_vertex + j;
                }
                AddTriangle(builder, tri_indices[0], tri_indices[1], tri_indices[2]);
            }
            break;
        }
    }

    if (remaining_vertices == 3) {
        u16 tri_indices[3];
        for (u16 i = 0; i < f->vertex_count; i++) {
            if (f->vertices[i] == indices[0])
                tri_indices[0] = base_vertex + i;
            if (f->vertices[i] == indices[1])
                tri_indices[1] = base_vertex + i;
            if (f->vertices[i] == indices[2])
                tri_indices[2] = base_vertex + i;
        }
        AddTriangle(builder, tri_indices[0], tri_indices[1], tri_indices[2]);
    }
}

int GetSelectedVertices(MeshData* m, int vertices[MAX_VERTICES]) {
    int selected_vertex_count=0;
    for (int select_index=0; select_index<m->vertex_count; select_index++) {
        VertexData& v = m->vertices[select_index];
        if (!v.selected) continue;
        vertices[selected_vertex_count++] = select_index;
    }
    return selected_vertex_count;
}

int GetSelectedEdges(MeshData* m, int edges[MAX_EDGES]) {
    int selected_edge_count=0;
    for (int edge_index=0; edge_index<m->edge_count; edge_index++) {
        EdgeData& e = m->edges[edge_index];
        if (!e.selected)
            continue;

        edges[selected_edge_count++] = edge_index;
    }

    return selected_edge_count;
}

void AddTag(MeshData* m, const Vec2& position) {
    if (m->tag_count >= MESH_MAX_TAGS)
        return;

    m->tags[m->tag_count++].position = position;
}

void RemoveTag(MeshData* m, int index) {
    assert(index >= 0 && index < m->tag_count);
    for (int tag_index = index; tag_index < m->tag_count - 1; tag_index++)
        m->tags[tag_index] = m->tags[tag_index + 1];

    m->tag_count--;
}

int HitTestTag(MeshData* m, const Vec2& position, float size_mult) {
    float size = g_view.select_size * size_mult;
    float best_dist = F32_MAX;
    int best_index = -1;
    for (int i = 0; i < m->tag_count; i++) {
        const TagData& t = m->tags[i];
        float dist = Length(position - t.position);
        if (dist < size && dist < best_dist) {
            best_index = i;
            best_dist = dist;
        }
    }

    return best_index;
}

Vec2 HitTestSnap(MeshData* m, const Vec2& position) {
    float best_dist_sqr = LengthSqr(position);
    Vec2 best_snap = VEC2_ZERO;

    for (int i = 0; i < m->tag_count; i++) {
        const TagData& t = m->tags[i];
        float dist_sqr = DistanceSqr(t.position, position);
        if (dist_sqr < best_dist_sqr) {
            best_dist_sqr = dist_sqr;
            best_snap = t.position;
        }
    }

    return best_snap;
}

Vec2 GetEdgePoint(MeshData* m, int edge_index, float t) {
    return Mix(
        m->vertices[m->edges[edge_index].v0].position,
        m->vertices[m->edges[edge_index].v1].position,
        t);
}

void SetOrigin(MeshData* m, const Vec2& origin) {
    Vec2 delta = m->position - origin;
    for (int vertex_index = 0; vertex_index < m->vertex_count; vertex_index++)
        m->vertices[vertex_index].position += delta;

    for (int anchor_index = 0; anchor_index < m->tag_count; anchor_index++)
        m->tags[anchor_index].position += delta;

    m->position = origin;
    UpdateEdges(m);
    MarkDirty(m);
}

float GetVertexWeight(MeshData* m, int vertex_index, int bone_index) {
    if (bone_index < 0)
        return 0.0f;

    for (int weight_index = 0; weight_index < MESH_MAX_VERTEX_WEIGHTS; weight_index++) {
        const VertexWeight& w = m->vertices[vertex_index].weights[weight_index];
        if (w.bone_index == bone_index)
            return w.weight;
    }

    return 0.0f;
}

int GetVertexWeightIndex(MeshData* m, int vertex_index, int bone_index) {
    for (int weight_index = 0; weight_index < MESH_MAX_VERTEX_WEIGHTS; weight_index++) {
        const VertexWeight& w = m->vertices[vertex_index].weights[weight_index];
        if (w.bone_index == bone_index && w.weight > F32_EPSILON)
            return weight_index;
    }

    return -1;
}

int GetOrAddVertexWeightIndex(MeshData* m, int vertex_index, int bone_index) {
    int weight_index = GetVertexWeightIndex(m, vertex_index, bone_index);
    if (weight_index != -1)
        return weight_index;

    for (weight_index = 0; weight_index < MESH_MAX_VERTEX_WEIGHTS; weight_index++) {
        const VertexWeight& w = m->vertices[vertex_index].weights[weight_index];
        if (w.weight <= F32_EPSILON)
            return weight_index;
    }

    return -1;
}

void SetVertexWeight(MeshData* m, int vertex_index, int bone_index, float weight) {
    int weight_index = GetOrAddVertexWeightIndex(m, vertex_index, bone_index);
    if (weight_index == -1)
        return;

    VertexData& v = m->vertices[vertex_index];
    VertexWeight& w = v.weights[weight_index];
    w.bone_index = bone_index;
    w.weight = weight;
}

void AddVertexWeight(MeshData* m, int vertex_index, int bone_index, float weight) {
    int weight_index = GetOrAddVertexWeightIndex(m, vertex_index, bone_index);
    if (weight_index == -1)
        return;

    VertexData& v = m->vertices[vertex_index];
    VertexWeight& w = v.weights[weight_index];
    w.bone_index = bone_index;
    w.weight = Clamp01(w.weight + weight);
}

static void DestroyMeshData(AssetData* a) {
    MeshData* m = static_cast<MeshData*>(a);
    Free(m->data);
    m->data = nullptr;
}

static void Init(MeshData* m) {
    AllocateData(m);

    m->vtable = {
        .destructor = DestroyMeshData,
        .load = LoadMeshData,
        .save = SaveMeshData,
        .draw = DrawMesh,
        .clone = CloneMeshData
    };

    InitMeshEditor(m);
}
