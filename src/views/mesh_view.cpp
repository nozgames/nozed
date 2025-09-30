//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "editor_assets.h"

#include <editor.h>

constexpr float HEIGHT_MIN = -1.0f;
constexpr float HEIGHT_MAX = 1.0f;
constexpr float EDGE_MIN = 0.0f;
constexpr float EDGE_MAX = 2.0f;

constexpr float HEIGHT_SLIDER_SIZE = 2.0f;
constexpr float VERTEX_SIZE = 0.08f;
constexpr float VERTEX_HIT_SIZE = 20.0f;
constexpr float CIRCLE_CONTROL_OUTLINE_SIZE = 0.13f;
constexpr float CIRCLE_CONTROL_SIZE = 0.12f;
constexpr float CENTER_SIZE = 0.2f;
constexpr float ORIGIN_SIZE = 0.1f;
constexpr float ORIGIN_BORDER_SIZE = 0.12f;
constexpr float ROTATE_TOOL_WIDTH = 0.02f;

enum MeshEditorState
{
    MESH_EDITOR_STATE_DEFAULT,
    MESH_EDITOR_STATE_MOVE,
    MESH_EDITOR_STATE_ROTATE,
    MESH_EDITOR_STATE_SCALE,
    MESH_EDITOR_STATE_NORMAL,
    MESH_EDITOR_STATE_EDGE,
};

enum MeshEditorMode
{
    MESH_EDITOR_MODE_VERTEX,
    MESH_EDITOR_MODE_EDGE,
    MESH_EDITOR_MODE_FACE
};

struct MeshViewVertex
{
    float saved_height;
    float saved_edge_size;
    Vec2 saved_position;
};

struct MeshViewFace
{
    Vec3 saved_normal;
};

struct MeshView
{
    MeshEditorState state;
    MeshEditorMode mode;
    Vec2 world_drag_start;
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
    MeshViewVertex vertices[MAX_VERTICES];
    MeshViewFace faces[MAX_FACES];
    Mesh* rotate_arc_mesh;
};

static MeshView g_mesh_view = {};

inline EditorMesh* GetEditingMesh()
{
    EditorAsset* ea = GetEditingAsset();
    assert(ea->type == EDITOR_ASSET_TYPE_MESH);
    return (EditorMesh*)ea;
}

static void DrawVertices(bool selected)
{
    EditorAsset* ea = GetEditingAsset();
    EditorMesh* em = GetEditingMesh();
    for (int i=0; i<em->vertex_count; i++)
    {
        const EditorVertex& ev = em->vertices[i];
        if (ev.selected != selected)
            continue;
        DrawVertex(ev.position + ea->position);
    }
}

static void UpdateSelection()
{
    EditorMesh* em = GetEditingMesh();
    Vec2 center = VEC2_ZERO;
    Bounds2 bounds = { VEC2_ZERO, VEC2_ZERO };

    em->selected_count = 0;
    switch (g_mesh_view.mode)
    {
    case MESH_EDITOR_MODE_VERTEX:
        for (int vertex_index=0; vertex_index<em->vertex_count; vertex_index++)
        {
            const EditorVertex& ev = em->vertices[vertex_index];
            if (!ev.selected)
                continue;

            if (em->selected_count == 0)
                bounds = { ev.position, ev.position };
            else
                bounds = Union(bounds, ev.position);

            em->selected_count++;
        }
        break;

    case MESH_EDITOR_MODE_EDGE:
        for (int vertex_index=0; vertex_index<em->vertex_count; vertex_index++)
            em->vertices[vertex_index].selected = false;

        for (int edge_index=0; edge_index<em->edge_count; edge_index++)
        {
            const EditorEdge& ee = em->edges[edge_index];
            if (!ee.selected)
                continue;

            EditorVertex& v0 = em->vertices[ee.v0];
            EditorVertex& v1 = em->vertices[ee.v1];
            center += (v0.position + v1.position) * 0.5f;
            v0.selected = true;
            v1.selected = true;

            if (em->selected_count == 0)
                bounds = { v0.position, v0.position };

            bounds = Union(bounds, v0.position);
            bounds = Union(bounds, v1.position);

            em->selected_count++;
        }
        break;

    case MESH_EDITOR_MODE_FACE:
        for (int face_index=0; face_index<em->face_count; face_index++)
        {
            const EditorFace& ef = em->faces[face_index];
            if (!ef.selected)
                continue;

            Vec2 face_center = GetFaceCenter(em, face_index);
            if (em->selected_count == 0)
                bounds = {face_center, face_center};

            bounds = Union(bounds, face_center);

            for (int vertex_index=0; vertex_index<ef.vertex_count; vertex_index++)
                em->vertices[ef.vertex_offset + vertex_index].selected = true;

            em->selected_count++;
        }
        break;
    }

    if (em->selected_count > 0)
        center = GetCenter(bounds);

    g_mesh_view.selection_center = center;
}

static void ClearSelection()
{
    EditorMesh* em = GetEditingMesh();

    for (int vertex_index=0; vertex_index<em->vertex_count; vertex_index++)
        em->vertices[vertex_index].selected = false;

    for (int edge_index=0; edge_index<em->edge_count; edge_index++)
        em->edges[edge_index].selected = false;

    for (int face_index=0; face_index<em->face_count; face_index++)
        em->faces[face_index].selected = false;

    UpdateSelection();
}

static void SelectAll(EditorMesh* em)
{
    switch (g_mesh_view.mode)
    {
    case MESH_EDITOR_MODE_VERTEX:
        for (int i=0; i<em->vertex_count; i++)
            em->vertices[i].selected = true;
        break;

    case MESH_EDITOR_MODE_EDGE:
        for (int i=0; i<em->edge_count; i++)
            em->edges[i].selected = true;
        break;

    case MESH_EDITOR_MODE_FACE:
        for (int i=0; i<em->face_count; i++)
            em->faces[i].selected = true;
        break;
    }

    UpdateSelection();
}

static void SelectVertex(int vertex_index, bool selected)
{
    assert(g_mesh_view.mode == MESH_EDITOR_MODE_VERTEX);

    EditorMesh* em = GetEditingMesh();
    assert(vertex_index >= 0 && vertex_index < em->vertex_count);
    EditorVertex& ev = em->vertices[vertex_index];
    if (ev.selected == selected)
        return;

    ev.selected = selected;
    UpdateSelection();
}

static void SelectEdge(int edge_index, bool selected)
{
    if (g_mesh_view.mode == MESH_EDITOR_MODE_VERTEX)
    {
        EditorMesh* em = GetEditingMesh();
        assert(edge_index >= 0 && edge_index < em->edge_count);
        EditorEdge& ee = em->edges[edge_index];
        SelectVertex(ee.v0, selected);
        SelectVertex(ee.v1, selected);
        return;
    }

    assert(g_mesh_view.mode == MESH_EDITOR_MODE_EDGE);

    EditorMesh* em = GetEditingMesh();
    assert(edge_index >= 0 && edge_index < em->edge_count);
    EditorEdge& ee = em->edges[edge_index];
    if (ee.selected == selected)
        return;

    ee.selected = selected;
    UpdateSelection();
}

static void SelectFace(int face_index, bool selected)
{
    assert(g_mesh_view.mode == MESH_EDITOR_MODE_FACE);

    EditorMesh* em = GetEditingMesh();
    assert(face_index >= 0 && face_index < em->face_count);
    EditorFace& ef = em->faces[face_index];
    if (ef.selected == selected)
        return;

    ef.selected = selected;
    UpdateSelection();
}

static int GetFirstSelectedEdge()
{
    EditorMesh* em = GetEditingMesh();
    for (int i=0; i<em->edge_count; i++)
        if (em->edges[i].selected)
            return i;

    return -1;
}

static int GetFirstSelectedVertex()
{
    EditorMesh* em = GetEditingMesh();
    for (int i=0; i<em->vertex_count; i++)
        if (em->vertices[i].selected)
            return i;

    return -1;
}

static int GetNextSelectedVertex(int prev_vertex)
{
    EditorMesh* em = GetEditingMesh();
    for (int i=prev_vertex+1; i<em->vertex_count; i++)
        if (em->vertices[i].selected)
            return i;

    return -1;
}

static void RevertSavedState()
{
    EditorAsset* ea = GetEditingAsset();
    EditorMesh* em = GetEditingMesh();
    for (int i=0; i<em->vertex_count; i++)
    {
        EditorVertex& ev = em->vertices[i];
        MeshViewVertex& mvv = g_mesh_view.vertices[i];
        ev.position = mvv.saved_position;
        ev.height = mvv.saved_height;
        ev.edge_size = mvv.saved_edge_size;
    }

    for (int i=0; i<em->face_count; i++)
    {
        EditorFace& ef = em->faces[i];
        MeshViewFace& mvf = g_mesh_view.faces[i];
        ef.normal = mvf.saved_normal;
    }

    MarkDirty(em);
    MarkModified(ea);
    UpdateSelection();
}

static void UpdateNormalState()
{
    EditorAsset* ea = GetEditingAsset();
    EditorMesh* em = GetEditingMesh();

    Vec2 dir = Normalize(g_view.mouse_world_position - g_mesh_view.selection_drag_start);

    for (int i=0; i<em->face_count; i++)
    {
        EditorFace& ef = em->faces[i];
        if (!ef.selected)
            continue;

        Vec2 xy = dir;
        ef.normal = {xy.x, xy.y, 1.0f};
    }

    MarkDirty(em);
    MarkModified(ea);
}

static void UpdateEdgeState()
{
    EditorMesh* em = GetEditingMesh();
    float delta = (g_view.mouse_position.y - g_mesh_view.state_mouse.y) / (g_view.dpi * HEIGHT_SLIDER_SIZE);

    for (i32 i=0; i<em->vertex_count; i++)
    {
        EditorVertex& ev = em->vertices[i];
        if (!ev.selected)
            continue;

        MeshViewVertex& mvv = g_mesh_view.vertices[i];
        ev.edge_size = Clamp(g_mesh_view.use_fixed_value ? g_mesh_view.fixed_value : mvv.saved_edge_size - delta, EDGE_MIN, EDGE_MAX);
    }

    MarkDirty(em);
    MarkModified(GetEditingAsset());
}

static void UpdateScaleState(EditorAsset* ea)
{
    float delta =
        Length(g_view.mouse_world_position - g_mesh_view.selection_drag_start) -
        Length(g_mesh_view.world_drag_start - g_mesh_view.selection_drag_start);

    EditorMesh* em = GetEditingMesh();
    for (i32 i=0; i<em->vertex_count; i++)
    {
        EditorVertex& ev = em->vertices[i];
        if (!ev.selected)
            continue;

        MeshViewVertex& mvv = g_mesh_view.vertices[i];
        Vec2 dir = mvv.saved_position - g_mesh_view.selection_center;
        ev.position = g_mesh_view.selection_center + dir * (1.0f + delta);
    }

    UpdateEdges(em);
    MarkDirty(em);
    MarkModified(ea);
}

static void UpdateRotateState(EditorAsset* ea)
{
    Vec2 start_dir = g_mesh_view.world_drag_start - g_mesh_view.selection_drag_start;
    Vec2 current_dir = g_view.mouse_world_position - g_mesh_view.selection_drag_start;

    float start_angle = atan2f(start_dir.y, start_dir.x);
    float current_angle = atan2f(current_dir.y, current_dir.x);
    float rotation_angle = current_angle - start_angle;

    float cos_angle = cosf(rotation_angle);
    float sin_angle = sinf(rotation_angle);

    EditorMesh* em = GetEditingMesh();
    for (i32 i=0; i<em->vertex_count; i++)
    {
        EditorVertex& ev = em->vertices[i];
        if (!ev.selected)
            continue;

        MeshViewVertex& mvv = g_mesh_view.vertices[i];
        Vec2 relative_pos = mvv.saved_position - g_mesh_view.selection_center;

        // Apply rotation
        Vec2 rotated_pos;
        rotated_pos.x = relative_pos.x * cos_angle - relative_pos.y * sin_angle;
        rotated_pos.y = relative_pos.x * sin_angle + relative_pos.y * cos_angle;

        ev.position = g_mesh_view.selection_center + rotated_pos;
    }

    UpdateEdges(em);
    MarkDirty(em);
    MarkModified(ea);
}

static void UpdateMoveState()
{
    bool secondary = IsShiftDown(g_view.input);
    Vec2 delta = IsCtrlDown(g_view.input)
        ? SnapToGrid(g_view.mouse_world_position, secondary) - SnapToGrid(g_mesh_view.world_drag_start, secondary)
        : g_view.mouse_world_position - g_mesh_view.world_drag_start;

    const TextInput& text_input = GetTextInput();
    if (text_input.length > 0 && text_input.value[0] == 'x')
        delta.y = 0.0f;
    else if (text_input.length > 0 && text_input.value[0] == 'y')
        delta.x = 0.0f;

    EditorMesh* em = GetEditingMesh();
    for (int i=0; i<em->vertex_count; i++)
    {
        EditorVertex& ev = em->vertices[i];
        MeshViewVertex& mvv = g_mesh_view.vertices[i];
        if (ev.selected)
            ev.position = mvv.saved_position + delta;
    }

    UpdateEdges(em);
    MarkDirty(em);
    MarkModified(GetEditingAsset());
}

static void SetState(MeshEditorState state)
{
    EditorAsset* ea = GetEditingAsset();
    g_mesh_view.state = state;
    g_mesh_view.world_drag_start = g_view.mouse_world_position;
    g_mesh_view.state_mouse = g_view.mouse_position;
    g_mesh_view.selection_drag_start = ea->position + g_mesh_view.selection_center;
    g_mesh_view.use_fixed_value = false;
    g_mesh_view.use_negative_fixed_value = false;

    EditorMesh* em = GetEditingMesh();
    for (int i=0; i<em->vertex_count; i++)
    {
        MeshViewVertex& mvv = g_mesh_view.vertices[i];
        EditorVertex& ev = em->vertices[i];
        mvv.saved_position = ev.position;
        mvv.saved_edge_size = ev.edge_size;
        mvv.saved_height = ev.height;
    }

    for (int i=0; i<em->face_count; i++)
    {
        MeshViewFace& mvf = g_mesh_view.faces[i];
        EditorFace& ef = em->faces[i];
        mvf.saved_normal = ef.normal;
    }

    ClearTextInput();

    switch (state)
    {
    case MESH_EDITOR_STATE_MOVE:
    case MESH_EDITOR_STATE_ROTATE:
    case MESH_EDITOR_STATE_SCALE:
    case MESH_EDITOR_STATE_NORMAL:
    case MESH_EDITOR_STATE_EDGE:
        RecordUndo();
        break;

    default:
        break;
    }
}

static bool HandleSelectVertex()
{
    assert(g_mesh_view.mode == MESH_EDITOR_MODE_VERTEX);

    EditorAsset* ea = GetEditingAsset();
    EditorMesh* em = GetEditingMesh();
    int vertex_index = HitTestVertex(em, g_view.mouse_world_position - ea->position);
    if (vertex_index == -1)
        return false;

    if (IsCtrlDown(g_view.input) || IsShiftDown(g_view.input))
        SelectVertex(vertex_index, !em->vertices[vertex_index].selected);
    else
    {
        ClearSelection();
        SelectVertex(vertex_index, true);
    }

    return true;
}

static bool HandleSelectEdge()
{
    assert(g_mesh_view.mode == MESH_EDITOR_MODE_EDGE);

    EditorAsset* ea = GetEditingAsset();
    EditorMesh* em = GetEditingMesh();
    int edge_index = HitTestEdge(em, g_view.mouse_world_position - ea->position);
    if (edge_index == -1)
        return false;

    bool ctrl = IsCtrlDown(g_view.input);
    bool shift = IsShiftDown(g_view.input);

    if (!ctrl && !shift)
        ClearSelection();

    EditorEdge& ee = em->edges[edge_index];
    const EditorVertex& v0 = em->vertices[ee.v0];
    const EditorVertex& v1 = em->vertices[ee.v1];

    if ((!ctrl && !shift) || !v0.selected || !v1.selected)
        SelectEdge(edge_index, true);
    else
        SelectEdge(edge_index, false);

    return true;
}

static bool HandleSelectFace()
{
    assert(g_mesh_view.mode == MESH_EDITOR_MODE_FACE);

    EditorAsset* ea = GetEditingAsset();
    EditorMesh* em = GetEditingMesh();
    int face_index = HitTestFace(
        em,
        ea->position,
        g_view.mouse_world_position,
        nullptr);

    if (face_index == -1)
        return false;

    bool ctrl = IsCtrlDown(g_view.input);
    bool shift = IsShiftDown(g_view.input);

    if (!ctrl && !shift)
        ClearSelection();

    if ((!ctrl && !shift) || !em->faces[face_index].selected)
        SelectFace(face_index, true);
    else
        SelectFace(face_index, false);

    return true;
}

extern int SplitFaces(EditorMesh* em, int v0, int v1);

static void InsertVertexFaceOrEdge()
{
    if (g_mesh_view.state != MESH_EDITOR_STATE_DEFAULT)
        return;

    if (g_mesh_view.mode != MESH_EDITOR_MODE_VERTEX)
        return;

    EditorAsset* ea = GetEditingAsset();
    EditorMesh* em = GetEditingMesh();

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

static void DissolveSelected()
{
    EditorAsset* ea = GetEditingAsset();
    EditorMesh* em = GetEditingMesh();

    if (em->selected_count == 0)
        return;

    RecordUndo();

    switch (g_mesh_view.mode)
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

static void UpdateDefaultState()
{
    if (g_view.drag)
    {
        BeginBoxSelect(HandleBoxSelect);
        return;
    }

    // Select
    if (!g_mesh_view.ignore_up && !g_view.drag && WasButtonReleased(g_view.input, MOUSE_LEFT))
    {
        g_mesh_view.clear_selection_on_up = false;

        switch (g_mesh_view.mode)
        {
        case MESH_EDITOR_MODE_VERTEX:
            if (HandleSelectVertex())
                return;
            break;

        case MESH_EDITOR_MODE_EDGE:
            if (HandleSelectEdge())
                return;
            break;

        case MESH_EDITOR_MODE_FACE:
            if (HandleSelectFace())
                return;
            break;
        }

        g_mesh_view.clear_selection_on_up = true;
    }

    g_mesh_view.ignore_up &= !WasButtonReleased(g_view.input, MOUSE_LEFT);

    if (WasButtonReleased(g_view.input, MOUSE_LEFT) && g_mesh_view.clear_selection_on_up)
        ClearSelection();
}

static bool HandleColorPickerInput(const ElementInput& input)
{
    float x = (input.mouse_position.x - GetLeft(input.bounds)) / input.bounds.width;
    float y = (input.mouse_position.y - GetTop(input.bounds)) / input.bounds.height;
    if (x < 0 || x > 1 || y < 0 || y > 1)
        return false;

    i32 col = (i32)(x * 16.0f);
    i32 row = (i32)(y * 16.0f);

    RecordUndo();

    EditorAsset* ea = (EditorAsset*)input.user_data;
    if (IsCtrlDown(g_view.input))
        SetEdgeColor(GetEditingMesh(), { col, row });
    else
        SetSelectedTrianglesColor(GetEditingMesh(), { col, row });

    MarkModified(ea);
    return true;
}

void MeshViewUpdate()
{
    EditorAsset* ea = GetEditingAsset();

    BeginCanvas();
    Image(g_mesh_view.color_material, STYLE_MESH_EDITOR_COLORS);
    SetInputHandler(HandleColorPickerInput, &ea);
    EndCanvas();

    CheckShortcuts(g_mesh_view.shortcuts);

    switch (g_mesh_view.state)
    {
    case MESH_EDITOR_STATE_DEFAULT:
        UpdateDefaultState();
        return;

    case MESH_EDITOR_STATE_MOVE:
        UpdateMoveState();
        break;

    case MESH_EDITOR_STATE_ROTATE:
        UpdateRotateState(ea);
        break;

    case MESH_EDITOR_STATE_SCALE:
        UpdateScaleState(ea);
        break;

    case MESH_EDITOR_STATE_NORMAL:
        UpdateNormalState();
        break;

    case MESH_EDITOR_STATE_EDGE:
        UpdateEdgeState();
        break;

    default:
        break;
    }

    // Commit the tool
    if (WasButtonPressed(g_view.input, MOUSE_LEFT) || WasButtonPressed(g_view.input, KEY_ENTER))
    {
        UpdateSelection();
        g_mesh_view.ignore_up = true;
        g_mesh_view.state = MESH_EDITOR_STATE_DEFAULT;
    }
    // Cancel the tool
    else if (WasButtonPressed(g_view.input, KEY_ESCAPE) || WasButtonPressed(g_view.input, MOUSE_RIGHT))
    {
        CancelUndo();
        RevertSavedState();
        g_mesh_view.state = MESH_EDITOR_STATE_DEFAULT;
    }
}

static void DrawRotateState()
{
    Vec2 center = g_mesh_view.selection_drag_start;
    Vec2 start_dir = g_mesh_view.world_drag_start - center;
    Vec2 current_dir = g_view.mouse_world_position - center;

    float current_radius = Length(current_dir);
    float start_angle = atan2f(start_dir.y, start_dir.x);
    float current_angle = atan2f(current_dir.y, current_dir.x);
    float rotation_angle = current_angle - start_angle;

    // Normalize rotation angle to [-π, π] to avoid full circle jumps
    while (rotation_angle > noz::PI) rotation_angle -= noz::TWO_PI;
    while (rotation_angle < -noz::PI) rotation_angle += noz::TWO_PI;

    // Draw center point
    BindColor(SetAlpha(COLOR_CENTER, 0.75f));
    DrawVertex(center, CENTER_SIZE * 0.75f);

    // Draw start line extending to current radius
    Vec2 start_end = center + Normalize(start_dir) * current_radius;
    BindColor(SetAlpha(COLOR_CENTER, 0.1f));
    DrawLine(center, start_end);

    // Draw current line
    BindColor(COLOR_CENTER);
    DrawDashedLine(center, g_view.mouse_world_position);

    Free(g_mesh_view.rotate_arc_mesh);
    g_mesh_view.rotate_arc_mesh = nullptr;

    if (fabsf(rotation_angle) > 0.01f && current_radius > 0.01f)
    {
        BindColor(SetAlpha(COLOR_VERTEX, 0.1f));


        MeshBuilder* builder = CreateMeshBuilder(ALLOCATOR_DEFAULT, 128, 384);
        float arc_degrees = -Degrees(rotation_angle);
        if (arc_degrees < 0)
            AddArc(builder, VEC2_ZERO, current_radius, arc_degrees, 0.0f, 32, VEC2_ZERO);
        else
            AddArc(builder, VEC2_ZERO, current_radius, 0.0f, arc_degrees, 32, VEC2_ZERO);

        g_mesh_view.rotate_arc_mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE, true);
        DrawMesh(g_mesh_view.rotate_arc_mesh, TRS(center, Degrees(start_angle), VEC2_ONE));
        Free(builder);
    }

    BindColor(COLOR_ORIGIN);
    DrawVertex(g_view.mouse_world_position, CENTER_SIZE);
}

static void DrawScaleState()
{
    BindColor(SetAlpha(COLOR_CENTER, 0.75f));
    DrawVertex(g_mesh_view.selection_drag_start, CENTER_SIZE * 0.75f);
    BindColor(COLOR_CENTER);
    DrawLine(g_view.mouse_world_position, g_mesh_view.selection_drag_start, ROTATE_TOOL_WIDTH);
    BindColor(COLOR_ORIGIN);
    DrawVertex(g_view.mouse_world_position, CENTER_SIZE);
}

static void DrawCircleControls(float (*value_func)(const EditorVertex& ev))
{
    EditorAsset* ea = GetEditingAsset();
    EditorMesh* em = GetEditingMesh();

    for (int i=0; i<em->vertex_count; i++)
    {
        EditorVertex& ev = em->vertices[i];
        if (!ev.selected || !IsVertexOnOutsideEdge(em, i))
            continue;

        BindColor(COLOR_VERTEX_SELECTED);
        DrawMesh(g_view.circle_mesh, TRS(ev.position + ea->position, 0, VEC2_ONE * CIRCLE_CONTROL_OUTLINE_SIZE * g_view.zoom_ref_scale));
    }

    for (int i=0; i<em->vertex_count; i++)
    {
        EditorVertex& ev = em->vertices[i];
        if (!ev.selected || !IsVertexOnOutsideEdge(em, i))
            continue;

        f32 h = value_func(ev);
        i32 arc = Clamp((i32)(100 * h), 0, 100);

        BindColor(COLOR_BLACK);
        DrawMesh(g_view.circle_mesh, TRS(ev.position + ea->position, 0, VEC2_ONE * CIRCLE_CONTROL_SIZE * g_view.zoom_ref_scale));
        BindColor(COLOR_VERTEX_SELECTED);
        DrawMesh(g_view.arc_mesh[arc], TRS(ev.position + ea->position, 0, VEC2_ONE * CIRCLE_CONTROL_SIZE * g_view.zoom_ref_scale));
    }
}

static void DrawNormalState()
{
    BindColor(SetAlpha(COLOR_CENTER, 0.75f));
    DrawVertex(g_mesh_view.selection_drag_start, CENTER_SIZE * 0.75f);
    BindColor(COLOR_CENTER);
    DrawDashedLine(g_view.mouse_world_position, g_mesh_view.selection_drag_start);
    BindColor(COLOR_ORIGIN);
    DrawVertex(g_view.mouse_world_position, CENTER_SIZE);
}

static float GetEdgeSizeValue(const EditorVertex& ev)
{
    return (ev.edge_size - EDGE_MIN) / (EDGE_MAX - EDGE_MIN);
}

static void DrawEdgeState()
{
    DrawCircleControls(GetEdgeSizeValue);
}

void MeshViewDraw()
{
    EditorAsset* ea = GetEditingAsset();
    EditorMesh* em = GetEditingMesh();

    // Mesh
    BindColor(COLOR_WHITE);
    DrawMesh(em, Translate(ea->position));

    // Edges
    BindColor(COLOR_EDGE);
    DrawEdges(em, ea->position);

    switch (g_mesh_view.mode)
    {
    case MESH_EDITOR_MODE_VERTEX:
        BindColor(COLOR_VERTEX);
        DrawVertices(false);
        BindColor(COLOR_VERTEX_SELECTED);
        DrawVertices(true);
        break;

    case MESH_EDITOR_MODE_EDGE:
        BindColor(COLOR_EDGE_SELECTED);
        DrawSelectedEdges(em, ea->position);
        break;

    case MESH_EDITOR_MODE_FACE:
        BindColor(COLOR_VERTEX_SELECTED);
        DrawSelectedFaces(em, ea->position);
        DrawFaceCenters(em, ea->position);
        break;
    }

    // Tools
    switch (g_mesh_view.state)
    {
    case MESH_EDITOR_STATE_ROTATE:
        DrawRotateState();
        break;

    case MESH_EDITOR_STATE_SCALE:
        DrawScaleState();
        break;

    case MESH_EDITOR_STATE_NORMAL:
        DrawNormalState();
        break;

    case MESH_EDITOR_STATE_EDGE:
        DrawEdgeState();
        break;

    default:
        break;
    }
}

Bounds2 MeshViewBounds()
{
    EditorMesh* em = GetEditingMesh();
    Bounds2 bounds = BOUNDS2_ZERO;
    bool first = true;
    for (int i = 0; i < em->vertex_count; i++)
    {
        const EditorVertex& ev = em->vertices[i];
        if (!ev.selected)
            continue;

        if (first)
            bounds = {ev.position, ev.position};
        else
            bounds = Union(bounds, ev.position);

        first = false;
    }

    if (first)
        return GetBounds(GetEditingAsset());

    return bounds;
}

void HandleBoxSelect(const Bounds2& bounds)
{
    EditorAsset* ea = GetEditingAsset();
    EditorMesh* em = GetEditingMesh();

    bool shift = IsShiftDown(g_view.input);
    bool ctrl = IsCtrlDown(g_view.input);

    if (!shift && !ctrl)
        ClearSelection();

    switch (g_mesh_view.mode)
    {
    case MESH_EDITOR_MODE_VERTEX:
        for (int i=0; i<em->vertex_count; i++)
        {
            EditorVertex& ev = em->vertices[i];
            Vec2 vpos = ev.position + ea->position;

            if (vpos.x >= bounds.min.x && vpos.x <= bounds.max.x &&
                vpos.y >= bounds.min.y && vpos.y <= bounds.max.y)
            {
                if (!ctrl)
                    SelectVertex(i, true);
                else
                    SelectVertex(i, false);
            }
        }
        break;

    case MESH_EDITOR_MODE_EDGE:
        for (int edge_index=0; edge_index<em->edge_count; edge_index++)
        {
            EditorEdge& ee = em->edges[edge_index];
            Vec2 ev0 = em->vertices[ee.v0].position + ea->position;
            Vec2 ev1 = em->vertices[ee.v1].position + ea->position;
            if (Intersects(bounds, ev0, ev1))
            {
                if (!ctrl)
                    SelectEdge(edge_index, true);
                else
                    SelectEdge(edge_index, false);
            }
        }
        break;

    case MESH_EDITOR_MODE_FACE:
#if 0
        for (int face_index=0; face_index<em->face_count; face_index++)
        {
            EditorFace& ef = em->faces[face_index];
            Vec2 ev0 = em->vertices[ef.v0].position + ea->position;
            Vec2 ev1 = em->vertices[ef.v1].position + ea->position;
            Vec2 ev2 = em->vertices[ef.v2].position + ea->position;
            if (Intersects(bounds, ev0, ev1, ev2))
            {
                if (!ctrl)
                    SelectFace(face_index, true);
                else
                    SelectFace(face_index, false);
            }
        }
#endif
        break;

    default:
        break;
    }
}

static void HandleMoveCommand()
{
    if (g_mesh_view.state != MESH_EDITOR_STATE_DEFAULT)
        return;

    EditorMesh* em = GetEditingMesh();
    if (em->selected_count == 0)
        return;

    SetState(MESH_EDITOR_STATE_MOVE);
}

static void HandleRotateCommand()
{
    if (g_mesh_view.state != MESH_EDITOR_STATE_DEFAULT)
        return;

    EditorMesh* em = GetEditingMesh();
    if (em->selected_count == 0 || (g_mesh_view.mode == MESH_EDITOR_MODE_VERTEX && em->selected_count == 1))
        return;

    SetState(MESH_EDITOR_STATE_ROTATE);
}

static void HandleScaleCommand()
{
    if (g_mesh_view.state != MESH_EDITOR_STATE_DEFAULT)
        return;

    EditorMesh* em = GetEditingMesh();
    if (em->selected_count == 0)
        return;

    SetState(MESH_EDITOR_STATE_SCALE);
}

static void HandleNormalCommand()
{
    if (g_mesh_view.state != MESH_EDITOR_STATE_DEFAULT)
        return;

    EditorMesh* em = GetEditingMesh();
    if (em->selected_count == 0)
        return;

    SetState(MESH_EDITOR_STATE_NORMAL);
}

static void HandleEdgeCommand()
{
    if (g_mesh_view.state != MESH_EDITOR_STATE_DEFAULT)
        return;

    EditorMesh* em = GetEditingMesh();
    if (em->selected_count == 0)
        return;

    bool has_outside_edge = false;
    for (int i=0; i<em->vertex_count && !has_outside_edge; i++)
    {
        EditorVertex& ev = em->vertices[i];
        if (!ev.selected || !IsVertexOnOutsideEdge(em, i))
            continue;

        has_outside_edge = true;
    }

    if (!has_outside_edge)
        return;

    SetState(MESH_EDITOR_STATE_EDGE);
}

static void HandleSelectAllCommand()
{
    SelectAll(GetEditingMesh());
}

static void HandleTextInputChanged(EventId event_id, const void* event_data)
{
    (void) event_id;

    TextInput* text_input = (TextInput*)event_data;
    char* value = text_input->value;
    if (*value == 'x' || *value == 'y')
        value++;

    g_mesh_view.use_fixed_value = *value != 0;
    if (!g_mesh_view.use_fixed_value)
        return;

    try
    {
        g_mesh_view.fixed_value = std::stof(text_input->value);
    }
    catch (...)
    {
    }
}

static void MeshViewShutdown()
{
    Unlisten(EVENT_TEXTINPUT_CHANGED, HandleTextInputChanged);

    // Clean up arc mesh
    Free(g_mesh_view.rotate_arc_mesh);
    g_mesh_view.rotate_arc_mesh = nullptr;
}

static bool MeshViewAllowTextInput()
{
    return
        g_mesh_view.state == MESH_EDITOR_STATE_NORMAL ||
        g_mesh_view.state == MESH_EDITOR_STATE_MOVE ||
        g_mesh_view.state == MESH_EDITOR_STATE_EDGE;
}

static void SetVertexMode()
{
    g_mesh_view.mode = MESH_EDITOR_MODE_VERTEX;
}

static void SetEdgeMode()
{
    g_mesh_view.mode = MESH_EDITOR_MODE_EDGE;
}

static void SetFaceMode()
{
    g_mesh_view.mode = MESH_EDITOR_MODE_FACE;
}

static void CenterMesh()
{
    Center(GetEditingMesh());
}

static bool ExtrudeSelectedEdges(EditorMesh* em)
{
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
    for (int i = 0; i < selected_edge_count; i++)
    {
        const EditorEdge& ee = em->edges[selected_edges[i]];
        vertex_needs_extrusion[ee.v0] = true;
        vertex_needs_extrusion[ee.v1] = true;
    }

    // Create mapping from old vertex indices to new vertex indices
    int vertex_mapping[MAX_VERTICES];
    for (int i = 0; i < MAX_VERTICES; i++)
        vertex_mapping[i] = -1;

    // Create new vertices for each unique vertex that needs extrusion
    for (int i = 0; i < em->vertex_count; i++)
    {
        if (!vertex_needs_extrusion[i])
            continue;

        if (em->vertex_count >= MAX_VERTICES)
            return false;

        int new_vertex_index = em->vertex_count++;
        vertex_mapping[i] = new_vertex_index;

        // Copy vertex properties and offset position along edge normal
        EditorVertex& old_vertex = em->vertices[i];
        EditorVertex& new_vertex = em->vertices[new_vertex_index];

        new_vertex = old_vertex;

        // Don't offset the position - the new vertex should start at the same position
        // The user will move it in move mode
        new_vertex.selected = false;
    }

    // Store vertex pairs for the new edges we want to select
    int new_edge_vertex_pairs[MAX_EDGES][2];
    int new_edge_count = 0;

    // Create new edges for the extruded geometry
    for (int i = 0; i < selected_edge_count; i++)
    {
        const EditorEdge& original_edge = em->edges[selected_edges[i]];
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
            const EditorFace& ef = em->faces[face_idx];

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
        EditorFace& quad = em->faces[em->face_count++];
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
    for (int i = 0; i < new_edge_count; i++)
    {
        int v0 = new_edge_vertex_pairs[i][0];
        int v1 = new_edge_vertex_pairs[i][1];

        // Find the edge with these vertices
        for (int edge_idx = 0; edge_idx < em->edge_count; edge_idx++)
        {
            const EditorEdge& ee = em->edges[edge_idx];
            if ((ee.v0 == v0 && ee.v1 == v1) || (ee.v0 == v1 && ee.v1 == v0))
            {
                SelectEdge(edge_idx, true);
                break;
            }
        }
    }
    return true;
}


static void ExtrudeSelected()
{
    EditorMesh* em = GetEditingMesh();

    if (g_mesh_view.mode != MESH_EDITOR_MODE_EDGE || em->selected_count <= 0)
        return;

    RecordUndo();
    if (!ExtrudeSelectedEdges(em))
    {
        CancelUndo();
        return;
    }

    SetState(MESH_EDITOR_STATE_MOVE);
}

void MeshViewInit()
{
    EditorMesh* em = GetEditingMesh();

    Listen(EVENT_TEXTINPUT_CHANGED, HandleTextInputChanged);

    g_view.vtable = {
        .update = MeshViewUpdate,
        .draw = MeshViewDraw,
        .bounds = MeshViewBounds,
        .shutdown = MeshViewShutdown,
        .allow_text_input = MeshViewAllowTextInput
    };

    g_mesh_view.state = MESH_EDITOR_STATE_DEFAULT;
    g_mesh_view.mode = MESH_EDITOR_MODE_VERTEX;

    for (int i=0; i<em->vertex_count; i++)
        em->vertices[i].selected = false;

    if (!g_mesh_view.color_material)
    {
        g_mesh_view.color_material = CreateMaterial(ALLOCATOR_DEFAULT, SHADER_UI);
        SetTexture(g_mesh_view.color_material, TEXTURE_EDITOR_PALETTE, 0);
    }

    if (!g_mesh_view.shortcuts)
    {
        static Shortcut shortcuts[] = {
            { KEY_G, false, false, false, HandleMoveCommand },
            { KEY_R, false, false, false, HandleRotateCommand },
            { KEY_S, false, false, false, HandleScaleCommand },
            { KEY_Q, false, false, false, HandleNormalCommand },
            { KEY_W, false, false, false, HandleEdgeCommand },
            { KEY_A, false, false, false, HandleSelectAllCommand },
            { KEY_X, false, false, false, DissolveSelected },
            { KEY_V, false, false, false, InsertVertexFaceOrEdge },
            { KEY_1, false, false, false, SetVertexMode },
            { KEY_2, false, false, false, SetEdgeMode },
            { KEY_3, false, false, false, SetFaceMode },
            { KEY_C, false, false, false, CenterMesh },
            { KEY_E, false, false, false, ExtrudeSelected },
            { INPUT_CODE_NONE }
        };

        g_mesh_view.shortcuts = shortcuts;
        EnableShortcuts(shortcuts);
    }
}

