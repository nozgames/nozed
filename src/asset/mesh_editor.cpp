//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

constexpr float COLOR_PICKER_SIZE = 300.0f;
constexpr float COLOR_SQUARE_SIZE = COLOR_PICKER_SIZE / 16.0f;

enum MeshEditorMode {
    MESH_EDITOR_MODE_VERTEX,
    MESH_EDITOR_MODE_EDGE,
    MESH_EDITOR_MODE_FACE
};

struct MeshEditorVertex {
    float saved_edge_size;
    Vec2 saved_position;
};

struct MeshEditor {
    MeshEditorMode mode;
    Vec2 selection_drag_start;
    Vec2 selection_center;
    Material* color_material;
    bool clear_selection_on_up;
    Vec2 state_mouse;
    bool use_fixed_value;
    bool ignore_up;
    bool use_negative_fixed_value;
    float fixed_value;
    Shortcut* shortcuts;
    MeshEditorVertex vertices[MAX_VERTICES];
    InputSet* input;
};

static MeshEditor g_mesh_editor = {};

extern int SplitFaces(MeshData* m, int v0, int v1);
static void HandleBoxSelect(const Bounds2& bounds);

inline MeshData* GetMeshData() {
    AssetData* a = GetAssetData();
    assert(a->type == ASSET_TYPE_MESH);
    return (MeshData*)a;
}

static void DrawVertices(bool selected) {
    AssetData* a = GetAssetData();
    MeshData* m = GetMeshData();
    for (int i=0; i<m->vertex_count; i++)
    {
        const VertexData& v = m->vertices[i];
        if (v.selected != selected)
            continue;
        DrawVertex(v.position + a->position);
    }
}

static void UpdateSelection() {
    MeshData* m = GetMeshData();
    Vec2 center = VEC2_ZERO;
    Bounds2 bounds = { VEC2_ZERO, VEC2_ZERO };

    m->selected_count = 0;
    switch (g_mesh_editor.mode) {
    case MESH_EDITOR_MODE_VERTEX:
        for (int vertex_index=0; vertex_index<m->vertex_count; vertex_index++)
        {
            const VertexData& ev = m->vertices[vertex_index];
            if (!ev.selected)
                continue;

            if (m->selected_count == 0)
                bounds = { ev.position, ev.position };
            else
                bounds = Union(bounds, ev.position);

            m->selected_count++;
        }
        break;

    case MESH_EDITOR_MODE_EDGE:
        for (int vertex_index=0; vertex_index<m->vertex_count; vertex_index++)
            m->vertices[vertex_index].selected = false;

        for (int edge_index=0; edge_index<m->edge_count; edge_index++)
        {
            const EdgeData& ee = m->edges[edge_index];
            if (!ee.selected)
                continue;

            VertexData& v0 = m->vertices[ee.v0];
            VertexData& v1 = m->vertices[ee.v1];
            center += (v0.position + v1.position) * 0.5f;
            v0.selected = true;
            v1.selected = true;

            if (m->selected_count == 0)
                bounds = { v0.position, v0.position };

            bounds = Union(bounds, v0.position);
            bounds = Union(bounds, v1.position);

            m->selected_count++;
        }
        break;

    case MESH_EDITOR_MODE_FACE:
        for (int face_index=0; face_index<m->face_count; face_index++) {
            const FaceData& ef = m->faces[face_index];
            if (!ef.selected)
                continue;

            Vec2 face_center = GetFaceCenter(m, face_index);
            if (m->selected_count == 0)
                bounds = {face_center, face_center};

            bounds = Union(bounds, face_center);

            for (int vertex_index=0; vertex_index<ef.vertex_count; vertex_index++)
                m->vertices[m->face_vertices[ef.vertex_offset + vertex_index]].selected = true;

            m->selected_count++;
        }
        break;
    }

    if (m->selected_count > 0)
        center = GetCenter(bounds);

    g_mesh_editor.selection_center = center;
}

static void ClearSelection() {
    MeshData* m = GetMeshData();

    for (int vertex_index=0; vertex_index<m->vertex_count; vertex_index++)
        m->vertices[vertex_index].selected = false;

    for (int edge_index=0; edge_index<m->edge_count; edge_index++)
        m->edges[edge_index].selected = false;

    for (int face_index=0; face_index<m->face_count; face_index++)
        m->faces[face_index].selected = false;

    UpdateSelection();
}

static void SelectAll(MeshData* m) {
    if (g_mesh_editor.mode == MESH_EDITOR_MODE_VERTEX) {
        for (int i=0; i<m->vertex_count; i++)
            m->vertices[i].selected = true;
    } else if (g_mesh_editor.mode == MESH_EDITOR_MODE_EDGE) {
        for (int i=0; i<m->edge_count; i++)
            m->edges[i].selected = true;
    } else if (g_mesh_editor.mode == MESH_EDITOR_MODE_FACE) {
        for (int i=0; i<m->face_count; i++)
            m->faces[i].selected = true;
    }

    UpdateSelection();
}

static void SelectVertex(int vertex_index, bool selected) {
    assert(g_mesh_editor.mode == MESH_EDITOR_MODE_VERTEX);

    MeshData* m = GetMeshData();
    assert(vertex_index >= 0 && vertex_index < m->vertex_count);
    VertexData& ev = m->vertices[vertex_index];
    if (ev.selected == selected)
        return;

    ev.selected = selected;
    UpdateSelection();
}

static void SelectEdge(int edge_index, bool selected)
{
    if (g_mesh_editor.mode == MESH_EDITOR_MODE_VERTEX)
    {
        MeshData* em = GetMeshData();
        assert(edge_index >= 0 && edge_index < em->edge_count);
        EdgeData& ee = em->edges[edge_index];
        SelectVertex(ee.v0, selected);
        SelectVertex(ee.v1, selected);
        return;
    }

    assert(g_mesh_editor.mode == MESH_EDITOR_MODE_EDGE);

    MeshData* em = GetMeshData();
    assert(edge_index >= 0 && edge_index < em->edge_count);
    EdgeData& ee = em->edges[edge_index];
    if (ee.selected == selected)
        return;

    ee.selected = selected;
    UpdateSelection();
}

static void SelectFace(int face_index, bool selected) {
    assert(g_mesh_editor.mode == MESH_EDITOR_MODE_FACE);

    MeshData* em = GetMeshData();
    assert(face_index >= 0 && face_index < em->face_count);
    FaceData& ef = em->faces[face_index];
    if (ef.selected == selected)
        return;

    ef.selected = selected;
    UpdateSelection();
}

static int GetFirstSelectedEdge() {
    MeshData* m = GetMeshData();
    for (int i=0; i<m->edge_count; i++)
        if (m->edges[i].selected)
            return i;

    return -1;
}

static int GetFirstSelectedVertex() {
    MeshData* m = GetMeshData();
    for (int i=0; i<m->vertex_count; i++)
        if (m->vertices[i].selected)
            return i;

    return -1;
}

static int GetNextSelectedVertex(int prev_vertex) {
    MeshData* m = GetMeshData();
    for (int i=prev_vertex+1; i<m->vertex_count; i++)
        if (m->vertices[i].selected)
            return i;

    return -1;
}

static void SaveMeshState() {
    MeshData* m = GetMeshData();
    for (int i=0; i<m->vertex_count; i++) {
        MeshEditorVertex& ev = g_mesh_editor.vertices[i];
        VertexData& v = m->vertices[i];
        ev.saved_position = v.position;
        ev.saved_edge_size = v.edge_size;
    }
}

static void RevertMeshState() {
    AssetData* ea = GetAssetData();
    MeshData* em = GetMeshData();
    for (int i=0; i<em->vertex_count; i++) {
        VertexData& ev = em->vertices[i];
        MeshEditorVertex& mvv = g_mesh_editor.vertices[i];
        ev.position = mvv.saved_position;
        ev.edge_size = mvv.saved_edge_size;
    }

    MarkDirty(em);
    MarkModified(ea);
    UpdateSelection();
}

static bool TrySelectVertex() {
    assert(g_mesh_editor.mode == MESH_EDITOR_MODE_VERTEX);

    AssetData* a = GetAssetData();
    MeshData* m = GetMeshData();
    int vertex_index = HitTestVertex(m, g_view.mouse_world_position - a->position);
    if (vertex_index == -1)
        return false;

    if (IsShiftDown(g_mesh_editor.input)) {
        SelectVertex(vertex_index, !m->vertices[vertex_index].selected);
    } else {
        ClearSelection();
        SelectVertex(vertex_index, true);
    }

    return true;
}

static bool TrySelectEdge() {
    assert(g_mesh_editor.mode == MESH_EDITOR_MODE_EDGE);

    AssetData* ea = GetAssetData();
    MeshData* em = GetMeshData();
    int edge_index = HitTestEdge(em, g_view.mouse_world_position - ea->position);
    if (edge_index == -1)
        return false;

    bool ctrl = IsCtrlDown(g_mesh_editor.input);
    bool shift = IsShiftDown(g_mesh_editor.input);

    if (!ctrl && !shift)
        ClearSelection();

    EdgeData& ee = em->edges[edge_index];
    const VertexData& v0 = em->vertices[ee.v0];
    const VertexData& v1 = em->vertices[ee.v1];

    if ((!ctrl && !shift) || !v0.selected || !v1.selected)
        SelectEdge(edge_index, true);
    else
        SelectEdge(edge_index, false);

    return true;
}

static bool TrySelectFace() {
    assert(g_mesh_editor.mode == MESH_EDITOR_MODE_FACE);

    AssetData* a = GetAssetData();
    MeshData* m = GetMeshData();
    int face_index = HitTestFace(
        m,
        Translate(a->position),
        g_view.mouse_world_position,
        nullptr);

    if (face_index == -1)
        return false;

    bool shift = IsShiftDown(g_mesh_editor.input);

    if (!shift)
        ClearSelection();

    if (!shift || !m->faces[face_index].selected)
        SelectFace(face_index, true);
    else
        SelectFace(face_index, false);

    return true;
}

static void InsertVertexFaceOrEdge() {
    if (g_mesh_editor.mode != MESH_EDITOR_MODE_VERTEX)
        return;

    AssetData* ea = GetAssetData();
    MeshData* em = GetMeshData();

    RecordUndo();

    Vec2 position = g_view.mouse_world_position - ea->position;

    // Insert edge?
    if (em->selected_count == 2)
    {
        int v0 = GetFirstSelectedVertex();
        int v1 = GetNextSelectedVertex(v0);
        assert(v0 != -1 && v1 != -1);

        int edge_index = SplitFaces(em, v0, v1);
        if (edge_index == -1)
        {
            CancelUndo();
            return;
        }

        ClearSelection();
        SelectEdge(edge_index, true);
        return;
    }

    if (em->selected_count >= 3)
    {
        int face_index = CreateFace(em);
        if (face_index == -1)
            CancelUndo();

        return;
    }

    int vertex_index = HitTestVertex(em, position, 0.1f);
    if (vertex_index != -1)
        return;

    float edge_pos;
    int edge_index = HitTestEdge(em, position, &edge_pos);
    if (edge_index < 0)
        return;


    int new_vertex_index = SplitEdge(em, edge_index, edge_pos);
    if (new_vertex_index == -1)
        return;

    ClearSelection();
    SelectVertex(new_vertex_index, true);
}

static void DissolveSelected() {
    AssetData* ea = GetAssetData();
    MeshData* em = GetMeshData();

    if (em->selected_count == 0)
        return;

    RecordUndo();

    switch (g_mesh_editor.mode)
    {
    case MESH_EDITOR_MODE_VERTEX:
        DissolveSelectedVertices(em);
        break;

    case MESH_EDITOR_MODE_EDGE:
        if (em->selected_count > 1)
        {
            CancelUndo();
            return;
        }
        DissolveEdge(em, GetFirstSelectedEdge());
        ClearSelection();
        break;

    case MESH_EDITOR_MODE_FACE:
        DissolveSelectedFaces(em);
        break;
    }

    MarkDirty(em);
    MarkModified(ea);
    UpdateSelection();
}

static void UpdateDefaultState() {
    if (!IsToolActive() && g_view.drag_started) {
        BeginBoxSelect(HandleBoxSelect);
        return;
    }

    // Select
    if (!g_mesh_editor.ignore_up && !g_view.drag && WasButtonReleased(g_mesh_editor.input, MOUSE_LEFT)) {
        g_mesh_editor.clear_selection_on_up = false;

        switch (g_mesh_editor.mode)
        {
        case MESH_EDITOR_MODE_VERTEX:
            if (TrySelectVertex())
                return;
            break;

        case MESH_EDITOR_MODE_EDGE:
            if (TrySelectEdge())
                return;
            break;

        case MESH_EDITOR_MODE_FACE:
            if (TrySelectFace())
                return;
            break;
        }

        g_mesh_editor.clear_selection_on_up = true;
    }

    g_mesh_editor.ignore_up &= !WasButtonReleased(g_mesh_editor.input, MOUSE_LEFT);

    if (WasButtonReleased(g_mesh_editor.input, MOUSE_LEFT) && g_mesh_editor.clear_selection_on_up && !IsShiftDown(g_mesh_editor.input))
        ClearSelection();
}

static bool HandleColorPickerInput(const Vec2& position) {
    float x = position.x / COLOR_PICKER_SIZE;
    float y = position.y / COLOR_PICKER_SIZE;
    if (x < 0 || x > 1 || y < 0 || y > 1)
        return false;

    i32 col = (i32)(x * 16.0f);
    i32 row = (i32)(y * 16.0f);

    RecordUndo();

    if (IsCtrlDown(g_mesh_editor.input))
        SetEdgeColor(GetMeshData(), {col, row});
    else
        SetSelectedTrianglesColor(GetMeshData(), {col, row});

    MarkModified();
    return true;
}

static void UpdateColorPicker(){
    static bool selected_colors[256] = {};
    memset(selected_colors, 0, sizeof(selected_colors));
    MeshData* em = GetMeshData();
    for (int face_index=0; face_index<em->face_count; face_index++) {
        FaceData& ef = em->faces[face_index];
        if (!ef.selected)
            continue;

        selected_colors[ef.color.y * 16 + ef.color.x] = true;
    }

    Canvas([] {
        Align({.alignment = ALIGNMENT_BOTTOM_LEFT}, [] {
            Container({.width = COLOR_PICKER_SIZE, .height = COLOR_PICKER_SIZE, .margin = EdgeInsetsBottomLeft(10)}, [] {
                GestureDetector({.on_tap = [](const TapDetails& details, void*) {
                    if (HandleColorPickerInput(details.position)) {
                        ConsumeButton(MOUSE_LEFT);
                    }
                }}, [] {
                    Image(g_mesh_editor.color_material);
                });

                for (int i=0; i<256; i++) {
                    if (selected_colors[i]) {
                        Transformed({.translate=Vec2{(i % 16) * COLOR_SQUARE_SIZE, (i / 16) * COLOR_SQUARE_SIZE}}, [] {
                            SizedBox({.width=COLOR_SQUARE_SIZE, .height=COLOR_SQUARE_SIZE}, [] {
                                Border({.width=2,.color=COLOR_VERTEX_SELECTED});
                            });
                        });
                    }
                }
            });
        });
    });
}

static Bounds2 GetMeshEditorBounds() {
    MeshData* em = GetMeshData();
    Bounds2 bounds = BOUNDS2_ZERO;
    bool first = true;
    for (int i = 0; i < em->vertex_count; i++)
    {
        const VertexData& ev = em->vertices[i];
        if (!ev.selected)
            continue;

        if (first)
            bounds = {ev.position, ev.position};
        else
            bounds = Union(bounds, ev.position);

        first = false;
    }

    if (first)
        return GetBounds(GetAssetData());

    return bounds;
}

static void HandleBoxSelect(const Bounds2& bounds) {
    AssetData* ea = GetAssetData();
    MeshData* em = GetMeshData();

    bool shift = IsShiftDown();
    if (!shift)
        ClearSelection();

    switch (g_mesh_editor.mode) {
    case MESH_EDITOR_MODE_VERTEX:
        for (int i=0; i<em->vertex_count; i++)
        {
            VertexData& ev = em->vertices[i];
            Vec2 vpos = ev.position + ea->position;

            if (vpos.x >= bounds.min.x && vpos.x <= bounds.max.x &&
                vpos.y >= bounds.min.y && vpos.y <= bounds.max.y) {
                SelectVertex(i, true);
            }
        }
        break;

    case MESH_EDITOR_MODE_EDGE:
        for (int edge_index=0; edge_index<em->edge_count; edge_index++)
        {
            EdgeData& ee = em->edges[edge_index];
            Vec2 ev0 = em->vertices[ee.v0].position + ea->position;
            Vec2 ev1 = em->vertices[ee.v1].position + ea->position;
            if (Intersects(bounds, ev0, ev1)) {
                SelectEdge(edge_index, true);
            }
        }
        break;

    case MESH_EDITOR_MODE_FACE:
#if 0
        for (int face_index=0; face_index<em->face_count; face_index++)
        {
            FaceData& ef = em->faces[face_index];
            Vec2 ev0 = em->vertices[ef.v0].position + ea->position;
            Vec2 ev1 = em->vertices[ef.v1].position + ea->position;
            Vec2 ev2 = em->vertices[ef.v2].position + ea->position;
            if (Intersects(bounds, ev0, ev1, ev2)) {
                SelectFace(face_index, true);
            }
        }
#endif
        break;

    default:
        break;
    }
}

static void CancelMeshTool() {
    CancelUndo();
    RevertMeshState();
}

static void UpdateMoveTool(const Vec2& delta) {
    MeshData* m = GetMeshData();
    bool snap = IsCtrlDown(GetInputSet());
    for (int i=0; i<m->vertex_count; i++) {
        VertexData& v = m->vertices[i];
        MeshEditorVertex& mvv = g_mesh_editor.vertices[i];
        if (v.selected)
            v.position = snap ? SnapToGrid(m->position + mvv.saved_position + delta) - m->position : mvv.saved_position + delta;
    }

    UpdateEdges(m);
    MarkDirty(m);
    MarkModified();
}

static void BeginMoveTool() {
    MeshData* m = GetMeshData();
    if (m->selected_count == 0)
        return;

    SaveMeshState();
    RecordUndo();
    BeginMoveTool({.update=UpdateMoveTool, .cancel=CancelMeshTool});
}

static void UpdateRotateTool(float angle) {
    float cos_angle = Cos(Radians(angle));
    float sin_angle = Sin(Radians(angle));

    MeshData* m = GetMeshData();
    for (i32 i=0; i<m->vertex_count; i++) {
        VertexData& ev = m->vertices[i];
        if (!ev.selected)
            continue;

        MeshEditorVertex& mvv = g_mesh_editor.vertices[i];
        Vec2 relative_pos = mvv.saved_position - g_mesh_editor.selection_center;

        Vec2 rotated_pos;
        rotated_pos.x = relative_pos.x * cos_angle - relative_pos.y * sin_angle;
        rotated_pos.y = relative_pos.x * sin_angle + relative_pos.y * cos_angle;
        ev.position = g_mesh_editor.selection_center + rotated_pos;
    }

    UpdateEdges(m);
    MarkDirty(m);
    MarkModified();
}

static void BeginRotateTool() {
    MeshData* m = GetMeshData();
    if (m->selected_count == 0 || (g_mesh_editor.mode == MESH_EDITOR_MODE_VERTEX && m->selected_count == 1))
        return;

    SaveMeshState();
    RecordUndo();
    BeginRotateTool({.origin=g_mesh_editor.selection_center+m->position, .update=UpdateRotateTool, .cancel=CancelMeshTool});
}

static void UpdateScaleTool(float scale) {
    MeshData* m = GetMeshData();
    for (i32 i=0; i<m->vertex_count; i++) {
        VertexData& v = m->vertices[i];
        if (!v.selected)
            continue;

        MeshEditorVertex& ev = g_mesh_editor.vertices[i];
        Vec2 dir = ev.saved_position - g_mesh_editor.selection_center;
        v.position = g_mesh_editor.selection_center + dir * scale;
    }

    UpdateEdges(m);
    MarkDirty(m);
    MarkModified();
}

static void BeginScaleTool() {
    MeshData* m = GetMeshData();
    if (m->selected_count == 0)
        return;

    SaveMeshState();
    RecordUndo();
    BeginScaleTool({.origin=g_mesh_editor.selection_center+m->position, .update=UpdateScaleTool, .cancel=CancelMeshTool});
}

static void UpdateOutlineToolVertex(float weight, void* user_data) {
    VertexData* v = (VertexData*)user_data;
    v->edge_size = weight;
}

static void UpdateOutlineTool() {
    MeshData* m = GetMeshData();
    UpdateEdges(m);
    MarkDirty(m);
    MarkModified();
}

static void BeginOutlineTool() {
    MeshData* m = GetMeshData();
    if (m->selected_count == 0)
        return;

    WeightToolOptions options = {
        .vertex_count = 0,
        .min_weight = 0,
        .max_weight = 2,
        .update = UpdateOutlineTool,
        .cancel = CancelMeshTool,
        .update_vertex = UpdateOutlineToolVertex,
    };

    for (int i=0; i<m->vertex_count; i++) {
        VertexData& ev = m->vertices[i];
        if (!ev.selected) // || !IsVertexOnOutsideEdge(m, i))
            continue;

        options.vertices[options.vertex_count++] = {
            .position = ev.position + GetAssetData()->position,
            .weight = ev.edge_size,
            .user_data = &ev,
        };
    }

    if (!options.vertex_count)
        return;

    SaveMeshState();
    RecordUndo();
    BeginWeightTool(options);
}

static void UpdateOpacityToolVertex(float weight, void*) {
    MeshData* m = GetMeshData();
    m->opacity = weight;
}

static void BeginMeshOpacityTool() {
    MeshData* m = GetMeshData();
    WeightToolOptions options = {
        .vertex_count = 1,
        .vertices = {
            { .position = g_view.mouse_world_position, .weight = m->opacity  }
        },
        .min_weight = 0,
        .max_weight = 2,
        .cancel = CancelMeshTool,
        .update_vertex = UpdateOpacityToolVertex,
    };

    BeginWeightTool(options);
}

static void SelectAll()
{
    SelectAll(GetMeshData());
}

static bool MeshViewAllowTextInput() {
    return false;
}

static void SetVertexMode() {
    g_mesh_editor.mode = MESH_EDITOR_MODE_VERTEX;
}

static void SetEdgeMode() {
    g_mesh_editor.mode = MESH_EDITOR_MODE_EDGE;
}

static void SetFaceMode() {
    g_mesh_editor.mode = MESH_EDITOR_MODE_FACE;
}

static void CenterMesh() {
    Center(GetMeshData());
}

static void CircleMesh() {
    if (GetMeshData()->selected_count < 2)
        return;

    BeginSelectTool({.commit= [](const Vec2& position ) {
        MeshData* m = GetMeshData();
        // find average distance of selected points to the given world position
        Vec2 center = position - GetAssetData()->position;
        float total_distance = 0.0f;
        int count = 0;
        for (int i=0; i<m->vertex_count; i++) {
            VertexData& v = m->vertices[i];
            if (!v.selected)
                continue;
            total_distance += Length(v.position - center);
            count++;
        }

        float radius = total_distance / (float)count;

        RecordUndo(m);

        // move selected vertices to form a circle around the center point
        for (int i=0; i<m->vertex_count; i++) {
            VertexData& v = m->vertices[i];
            if (!v.selected)
                continue;
            Vec2 dir = Normalize(v.position - center);
            v.position = center + dir * radius;
        }

        UpdateEdges(m);
        MarkDirty(m);
        MarkModified();
    }});
}

static bool ExtrudeSelectedEdges(MeshData* em) {
    if (em->edge_count == 0)
        return false;

    int selected_edges[MAX_EDGES];
    int selected_edge_count = 0;
    for (int i = 0; i < em->edge_count; i++)
        if (em->edges[i].selected && selected_edge_count < MAX_EDGES)
            selected_edges[selected_edge_count++] = i;

    if (selected_edge_count == 0)
        return false;

    // Find all unique vertices that are part of selected edges
    bool vertex_needs_extrusion[MAX_VERTICES] = {};
    for (int i = 0; i < selected_edge_count; i++) {
        const EdgeData& ee = em->edges[selected_edges[i]];
        vertex_needs_extrusion[ee.v0] = true;
        vertex_needs_extrusion[ee.v1] = true;
    }

    // Create mapping from old vertex indices to new vertex indices
    int vertex_mapping[MAX_VERTICES];
    for (int i = 0; i < MAX_VERTICES; i++)
        vertex_mapping[i] = -1;

    // Create new vertices for each unique vertex that needs extrusion
    for (int i = 0; i < em->vertex_count; i++) {
        if (!vertex_needs_extrusion[i])
            continue;

        if (em->vertex_count >= MAX_VERTICES)
            return false;

        int new_vertex_index = em->vertex_count++;
        vertex_mapping[i] = new_vertex_index;

        // Copy vertex properties and offset position along edge normal
        VertexData& old_vertex = em->vertices[i];
        VertexData& new_vertex = em->vertices[new_vertex_index];

        new_vertex = old_vertex;

        // Don't offset the position - the new vertex should start at the same position
        // The user will move it in move mode
        new_vertex.selected = false;
    }

    // Store vertex pairs for the new edges we want to select
    int new_edge_vertex_pairs[MAX_EDGES][2];
    int new_edge_count = 0;

    // Create new edges for the extruded geometry
    for (int i = 0; i < selected_edge_count; i++) {
        const EdgeData& original_edge = em->edges[selected_edges[i]];
        int old_v0 = original_edge.v0;
        int old_v1 = original_edge.v1;
        int new_v0 = vertex_mapping[old_v0];
        int new_v1 = vertex_mapping[old_v1];

        if (new_v0 == -1 || new_v1 == -1)
            continue;

        // Create connecting edges between old and new vertices
        if (em->edge_count + 3 >= MAX_EDGES)
            return false;

        if (em->face_count + 2 >= MAX_FACES)
            return false;

        GetOrAddEdge(em, old_v0, new_v0, -1);
        GetOrAddEdge(em, old_v1, new_v1, -1);
        GetOrAddEdge(em, new_v0, new_v1, -1);

        // Store the vertex pair for the new edge we want to select
        if (new_edge_count < MAX_EDGES)
        {
            new_edge_vertex_pairs[new_edge_count][0] = new_v0;
            new_edge_vertex_pairs[new_edge_count][1] = new_v1;
            new_edge_count++;
        }

        // Find the face that contains this edge to inherit its color and determine orientation
        Vec2Int face_color = {1, 0}; // Default color
        Vec3 face_normal = {0, 0, 1}; // Default normal
        bool edge_reversed = false; // Track if edge direction is reversed in the face
        bool found_face = false;

        for (int face_idx = 0; !found_face && face_idx < em->face_count; face_idx++)
        {
            const FaceData& ef = em->faces[face_idx];

            // Check if this face contains the edge using the face_vertices array
            for (int vertex_index = 0; !found_face && vertex_index < ef.vertex_count; vertex_index++)
            {
                int v0_idx = em->face_vertices[ef.vertex_offset + vertex_index];
                int v1_idx = em->face_vertices[ef.vertex_offset + (vertex_index + 1) % ef.vertex_count];

                if (v0_idx == old_v0 && v1_idx == old_v1)
                {
                    face_color = ef.color;
                    face_normal = ef.normal;
                    edge_reversed = false;
                    found_face = true;
                }
                else if (v0_idx == old_v1 && v1_idx == old_v0)
                {
                    face_color = ef.color;
                    face_normal = ef.normal;
                    edge_reversed = true;
                    found_face = true;
                }
            }
        }

        // Create a quad face for the extruded geometry using the new polygon structure
        FaceData& quad = em->faces[em->face_count++];
        quad.color = face_color;
        quad.normal = face_normal;
        quad.selected = false;

        // Set up quad vertices in the face_vertices array with correct winding
        quad.vertex_offset = em->face_vertex_count;
        quad.vertex_count = 4;

        if (!edge_reversed)
        {
            // Normal orientation: edge goes from old_v0 to old_v1 in the face
            // Quad vertices: old_v0, new_v0, new_v1, old_v1 (counter-clockwise for outward facing)
            em->face_vertices[em->face_vertex_count++] = old_v0;
            em->face_vertices[em->face_vertex_count++] = new_v0;
            em->face_vertices[em->face_vertex_count++] = new_v1;
            em->face_vertices[em->face_vertex_count++] = old_v1;
        }
        else
        {
            // Reversed orientation: edge goes from old_v1 to old_v0 in the face
            // Quad vertices: old_v1, new_v1, new_v0, old_v0 (counter-clockwise for outward facing)
            em->face_vertices[em->face_vertex_count++] = old_v1;
            em->face_vertices[em->face_vertex_count++] = new_v1;
            em->face_vertices[em->face_vertex_count++] = new_v0;
            em->face_vertices[em->face_vertex_count++] = old_v0;
        }
    }

    UpdateEdges(em);
    MarkDirty(em);

    // Clear all selections first
    ClearSelection();

    // Find and select the new edges by their vertex pairs
    for (int i = 0; i < new_edge_count; i++) {
        int v0 = new_edge_vertex_pairs[i][0];
        int v1 = new_edge_vertex_pairs[i][1];

        // Find the edge with these vertices
        for (int edge_idx = 0; edge_idx < em->edge_count; edge_idx++) {
            const EdgeData& ee = em->edges[edge_idx];
            if ((ee.v0 == v0 && ee.v1 == v1) || (ee.v0 == v1 && ee.v1 == v0)) {
                SelectEdge(edge_idx, true);
                break;
            }
        }
    }
    return true;
}

static void ExtrudeSelected() {
    MeshData* m = GetMeshData();

    if (g_mesh_editor.mode != MESH_EDITOR_MODE_EDGE || m->selected_count <= 0)
        return;

    RecordUndo();
    if (!ExtrudeSelectedEdges(m)) {
        CancelUndo();
        return;
    }

    BeginMoveTool();
}

static void AddNewFace() {
    RecordUndo();

    MeshData* em = GetMeshData();
    em->vertex_count += 4;
    em->vertices[em->vertex_count - 4] = { .position = { -0.25f, -0.25f }, .edge_size = 1.0f };
    em->vertices[em->vertex_count - 3] = { .position = {  0.25f, -0.25f }, .edge_size = 1.0f };
    em->vertices[em->vertex_count - 2] = { .position = {  0.25f,  0.25f }, .edge_size = 1.0f };
    em->vertices[em->vertex_count - 1] = { .position = { -0.25f,  0.25f }, .edge_size = 1.0f };
    em->faces[em->face_count++] = {
        .color = { 0, 0 },
        .normal = { 0, 0, 0 },
        .vertex_offset = em->face_vertex_count,
        .vertex_count = 4
    };
    em->face_vertices[em->face_vertex_count + 0] = em->vertex_count - 4;
    em->face_vertices[em->face_vertex_count + 1] = em->vertex_count - 3;
    em->face_vertices[em->face_vertex_count + 2] = em->vertex_count - 2;
    em->face_vertices[em->face_vertex_count + 3] = em->vertex_count - 1;
    em->face_vertex_count += 4;

    UpdateEdges(em);
    MarkDirty(em);
    MarkModified();
    ClearSelection();
    SelectVertex(em->vertex_count - 4, true);
    SelectVertex(em->vertex_count - 3, true);
    SelectVertex(em->vertex_count - 2, true);
    SelectVertex(em->vertex_count - 1, true);
}

static void BeginMeshEditor() {
    g_view.vtable = {
        .allow_text_input = MeshViewAllowTextInput
    };

    PushInputSet(g_mesh_editor.input);

    ClearSelection();

    g_mesh_editor.mode = MESH_EDITOR_MODE_VERTEX;
}

static void EndMeshEditor() {
    PopInputSet();
}

void ShutdownMeshEditor() {
    g_mesh_editor = {};
}


static void UpdateMeshEditor() {
    UpdateColorPicker();
    CheckShortcuts(g_mesh_editor.shortcuts, g_mesh_editor.input);
    UpdateDefaultState();
}

static void DrawMeshEditor() {
    AssetData* a = GetAssetData();
    MeshData* m = GetMeshData();

    // Mesh
    BindColor(SetAlpha(COLOR_WHITE, m->opacity));
    DrawMesh(m, Translate(a->position));

    // Edges
    BindColor(COLOR_EDGE);
    DrawEdges(m, a->position);

    switch (g_mesh_editor.mode)
    {
    case MESH_EDITOR_MODE_VERTEX:
        BindColor(COLOR_VERTEX);
        DrawVertices(false);
        BindColor(COLOR_VERTEX_SELECTED);
        DrawVertices(true);
        break;

    case MESH_EDITOR_MODE_EDGE:
        BindColor(COLOR_EDGE_SELECTED);
        DrawSelectedEdges(m, a->position);
        break;

    case MESH_EDITOR_MODE_FACE:
        BindColor(COLOR_VERTEX_SELECTED);
        DrawSelectedFaces(m, a->position);
        DrawFaceCenters(m, a->position);
        break;
    }
}

void UpdateMeshEditorPalette() {
    SetTexture(g_mesh_editor.color_material, g_view.palettes[g_view.active_palette_index].texture->texture, 0);
}

void InitMeshEditor() {
    g_mesh_editor.color_material = CreateMaterial(ALLOCATOR_DEFAULT, SHADER_UI);
    if (g_view.palette_count > 0)
        SetTexture(g_mesh_editor.color_material, g_view.palettes[0].texture->texture, 0);

    static Shortcut shortcuts[] = {
        { KEY_G, false, false, false, BeginMoveTool },
        { KEY_R, false, false, false, BeginRotateTool },
        { KEY_S, false, false, false, BeginScaleTool },
        { KEY_W, false, false, false, BeginOutlineTool },
        { KEY_O, false, false, false, BeginMeshOpacityTool },
        { KEY_A, false, false, false, SelectAll },
        { KEY_X, false, false, false, DissolveSelected },
        { KEY_V, false, false, false, InsertVertexFaceOrEdge },
        { KEY_1, false, false, false, SetVertexMode },
        { KEY_2, false, false, false, SetEdgeMode },
        { KEY_3, false, false, false, SetFaceMode },
        { KEY_C, false, false, false, CenterMesh },
        { KEY_C, false, false, true, CircleMesh },
        { KEY_E, false, false, false, ExtrudeSelected },
        { KEY_N, false, false, false, AddNewFace },
        { INPUT_CODE_NONE }
    };

    g_mesh_editor.input = CreateInputSet(ALLOCATOR_DEFAULT);
    EnableModifiers(g_mesh_editor.input);
    EnableButton(g_mesh_editor.input, MOUSE_LEFT);

    g_mesh_editor.shortcuts = shortcuts;
    EnableShortcuts(shortcuts, g_mesh_editor.input);
    EnableCommonShortcuts(g_mesh_editor.input);
}

void InitMeshEditor(MeshData* m) {
    m->vtable.editor_begin = BeginMeshEditor;
    m->vtable.editor_end = EndMeshEditor;
    m->vtable.editor_draw = DrawMeshEditor;
    m->vtable.editor_update = UpdateMeshEditor;
    m->vtable.editor_bounds = GetMeshEditorBounds;
}