//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "editor_assets.h"

#include <editor.h>

extern Vec2 SnapToGrid(const Vec2& position, bool secondary);

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
    bool use_negative_fixed_value;
    float fixed_value;
    Shortcut* shortcuts;
    MeshViewVertex vertices[MAX_VERTICES];
    MeshViewFace faces[MAX_TRIANGLES];
};

static MeshView g_mesh_view = {};

inline EditorMesh& GetEditingMesh() { return GetEditingAsset().mesh; }

static void DrawVertices(bool selected)
{
    const EditorAsset& ea = GetEditingAsset();
    const EditorMesh& em = GetEditingMesh();
    for (int i=0; i<em.vertex_count; i++)
    {
        const EditorVertex& ev = em.vertices[i];
        if (ev.selected != selected)
            continue;
        DrawVertex(ev.position + ea.position);
    }
}

static void UpdateSelection()
{
    EditorMesh& em = GetEditingMesh();
    Vec2 center = VEC2_ZERO;
    Bounds2 bounds = { VEC2_ZERO, VEC2_ZERO };

    em.selected_count = 0;
    switch (g_mesh_view.mode)
    {
    case MESH_EDITOR_MODE_VERTEX:
        for (int vertex_index=0; vertex_index<em.vertex_count; vertex_index++)
        {
            const EditorVertex& ev = em.vertices[vertex_index];
            if (!ev.selected)
                continue;

            if (em.selected_count == 0)
                bounds = { ev.position, ev.position };
            else
                bounds = Union(bounds, ev.position);

            em.selected_count++;
        }
        break;

    case MESH_EDITOR_MODE_EDGE:
        for (int vertex_index=0; vertex_index<em.vertex_count; vertex_index++)
            em.vertices[vertex_index].selected = false;

        for (int edge_index=0; edge_index<em.edge_count; edge_index++)
        {
            const EditorEdge& ee = em.edges[edge_index];
            if (!ee.selected)
                continue;

            EditorVertex& v0 = em.vertices[ee.v0];
            EditorVertex& v1 = em.vertices[ee.v1];
            center += (v0.position + v1.position) * 0.5f;
            v0.selected = true;
            v1.selected = true;

            if (em.selected_count == 0)
                bounds = { v0.position, v0.position };

            bounds = Union(bounds, v0.position);
            bounds = Union(bounds, v1.position);

            em.selected_count++;
        }
        break;

    case MESH_EDITOR_MODE_FACE:
        for (int face_index=0; face_index<em.face_count; face_index++)
        {
            const EditorFace& ef = em.faces[face_index];
            if (!ef.selected)
                continue;

            EditorVertex& v0 = em.vertices[ef.v0];
            EditorVertex& v1 = em.vertices[ef.v1];
            EditorVertex& v2 = em.vertices[ef.v2];

            if (em.selected_count == 0)
                bounds = { v0.position, v0.position };

            bounds = Union(bounds, v0.position);
            bounds = Union(bounds, v1.position);
            bounds = Union(bounds, v2.position);
            em.selected_count++;
            v0.selected = true;
            v1.selected = true;
            v2.selected = true;
        }
        break;
    }

    if (em.selected_count > 0)
        center = GetCenter(bounds);

    g_mesh_view.selection_center = center;
}

static void ClearSelection()
{
    EditorMesh& em = GetEditingMesh();

    for (int vertex_index=0; vertex_index<em.vertex_count; vertex_index++)
        em.vertices[vertex_index].selected = false;

    for (int edge_index=0; edge_index<em.edge_count; edge_index++)
        em.edges[edge_index].selected = false;

    for (int face_index=0; face_index<em.face_count; face_index++)
        em.faces[face_index].selected = false;

    UpdateSelection();
}

static void SelectAll(EditorMesh& em)
{
    switch (g_mesh_view.mode)
    {
    case MESH_EDITOR_MODE_VERTEX:
        for (int i=0; i<em.vertex_count; i++)
            em.vertices[i].selected = true;
        break;

    case MESH_EDITOR_MODE_EDGE:
        for (int i=0; i<em.edge_count; i++)
            em.edges[i].selected = true;
        break;

    case MESH_EDITOR_MODE_FACE:
        for (int i=0; i<em.face_count; i++)
            em.faces[i].selected = true;
        break;
    }

    UpdateSelection();
}

static void SelectVertex(int vertex_index, bool selected)
{
    assert(g_mesh_view.mode == MESH_EDITOR_MODE_VERTEX);

    EditorMesh& em = GetEditingMesh();
    assert(vertex_index >= 0 && vertex_index < em.vertex_count);
    EditorVertex& ev = em.vertices[vertex_index];
    if (ev.selected == selected)
        return;

    ev.selected = selected;
    UpdateSelection();
}

static void SelectEdge(int edge_index, bool selected)
{
    assert(g_mesh_view.mode == MESH_EDITOR_MODE_EDGE);

    EditorMesh& em = GetEditingMesh();
    assert(edge_index >= 0 && edge_index < em.edge_count);
    EditorEdge& ee = em.edges[edge_index];
    if (ee.selected == selected)
        return;

    ee.selected = selected;
    UpdateSelection();
}

static void SelectFace(int face_index, bool selected)
{
    assert(g_mesh_view.mode == MESH_EDITOR_MODE_FACE);

    EditorMesh& em = GetEditingMesh();
    assert(face_index >= 0 && face_index < em.face_count);
    EditorFace& ef = em.faces[face_index];
    if (ef.selected == selected)
        return;

    ef.selected = selected;
    UpdateSelection();
}

static int GetFirstSelectedEdge()
{
    EditorMesh& em = GetEditingMesh();
    for (int i=0; i<em.edge_count; i++)
        if (em.edges[i].selected)
            return i;

    return -1;
}

static void RevertSavedState()
{
    EditorAsset& ea = GetEditingAsset();
    EditorMesh& em = GetEditingMesh();
    for (int i=0; i<em.vertex_count; i++)
    {
        EditorVertex& ev = em.vertices[i];
        MeshViewVertex& mvv = g_mesh_view.vertices[i];
        ev.position = mvv.saved_position;
        ev.height = mvv.saved_height;
        ev.edge_size = mvv.saved_edge_size;
    }

    for (int i=0; i<em.face_count; i++)
    {
        EditorFace& ef = em.faces[i];
        MeshViewFace& mvf = g_mesh_view.faces[i];
        ef.normal = mvf.saved_normal;
    }

    MarkDirty(em);
    MarkModified(ea);
    UpdateSelection();
}

static void UpdateNormalState()
{
    EditorAsset& ea = GetEditingAsset();
    EditorMesh& em = GetEditingMesh();

    Vec2 dir = Normalize(g_view.mouse_world_position - g_mesh_view.selection_drag_start);

    for (int i=0; i<em.face_count; i++)
    {
        EditorFace& ef = em.faces[i];
        const EditorVertex& v0 = em.vertices[ef.v0];
        const EditorVertex& v1 = em.vertices[ef.v1];
        const EditorVertex& v2 = em.vertices[ef.v2];

        if (!v0.selected || !v1.selected || !v2.selected)
            continue;

        Vec2 xy = dir;
        ef.normal = {xy.x, xy.y, 1.0f};
    }

    MarkDirty(em);
    MarkModified(ea);
}

static void UpdateEdgeState()
{
    EditorMesh& em = GetEditingMesh();
    float delta = (g_view.mouse_position.y - g_mesh_view.state_mouse.y) / (g_view.dpi * HEIGHT_SLIDER_SIZE);

    for (i32 i=0; i<em.vertex_count; i++)
    {
        EditorVertex& ev = em.vertices[i];
        if (!ev.selected)
            continue;

        MeshViewVertex& mvv = g_mesh_view.vertices[i];
        ev.edge_size = Clamp(g_mesh_view.use_fixed_value ? g_mesh_view.fixed_value : mvv.saved_edge_size - delta, EDGE_MIN, EDGE_MAX);
    }

    MarkDirty(em);
    MarkModified(GetEditingAsset());
}

static void UpdateScaleState(EditorAsset& ea)
{
    float delta =
        Length(g_view.mouse_world_position - g_mesh_view.selection_drag_start) -
        Length(g_mesh_view.world_drag_start - g_mesh_view.selection_drag_start);

    EditorMesh& em = GetEditingMesh();
    for (i32 i=0; i<em.vertex_count; i++)
    {
        EditorVertex& ev = em.vertices[i];
        if (!ev.selected)
            continue;

        MeshViewVertex& mvv = g_mesh_view.vertices[i];
        Vec2 dir = mvv.saved_position - g_mesh_view.selection_center;
        ev.position = g_mesh_view.selection_center + dir * (1.0f + delta);
    }

    UpdateEdges(em);
    MarkDirty(em);
    MarkModified(ea);

    if (WasButtonPressed(g_view.input, MOUSE_LEFT))
    {
        UpdateSelection();
        g_mesh_view.state = MESH_EDITOR_STATE_DEFAULT;
    }
    else if (WasButtonPressed(g_view.input, MOUSE_RIGHT))
    {
        RevertSavedState();
        g_mesh_view.state = MESH_EDITOR_STATE_DEFAULT;
    }
}

static void UpdateMoveState()
{
    Vec2 delta = g_view.mouse_world_position - g_mesh_view.world_drag_start;

    EditorMesh& em = GetEditingMesh();
    for (int i=0; i<em.vertex_count; i++)
    {
        EditorVertex& ev = em.vertices[i];
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
    EditorAsset& ea = GetEditingAsset();
    g_mesh_view.state = state;
    g_mesh_view.world_drag_start = g_view.mouse_world_position;
    g_mesh_view.state_mouse = g_view.mouse_position;
    g_mesh_view.selection_drag_start = ea.position + g_mesh_view.selection_center;
    g_mesh_view.use_fixed_value = false;
    g_mesh_view.use_negative_fixed_value = false;

    EditorMesh& em = GetEditingMesh();
    for (int i=0; i<em.vertex_count; i++)
    {
        MeshViewVertex& mvv = g_mesh_view.vertices[i];
        EditorVertex& ev = em.vertices[i];
        mvv.saved_position = ev.position;
        mvv.saved_edge_size = ev.edge_size;
        mvv.saved_height = ev.height;
    }

    for (int i=0; i<em.face_count; i++)
    {
        MeshViewFace& mvf = g_mesh_view.faces[i];
        EditorFace& ef = em.faces[i];
        mvf.saved_normal = ef.normal;
    }

    ClearTextInput();

    switch (state)
    {
    case MESH_EDITOR_STATE_MOVE:
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

    EditorAsset& ea = GetEditingAsset();
    EditorMesh& em = GetEditingMesh();
    int vertex_index = HitTestVertex(em, g_view.mouse_world_position - ea.position);
    if (vertex_index == -1)
        return false;

    if (IsCtrlDown(g_view.input) || IsShiftDown(g_view.input))
        SelectVertex(vertex_index, !em.vertices[vertex_index].selected);
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

    EditorAsset& ea = GetEditingAsset();
    EditorMesh& em = GetEditingMesh();
    int edge_index = HitTestEdge(em, g_view.mouse_world_position - ea.position);
    if (edge_index == -1)
        return false;

    bool ctrl = IsCtrlDown(g_view.input);
    bool shift = IsShiftDown(g_view.input);

    if (!ctrl && !shift)
        ClearSelection();

    EditorEdge& ee = em.edges[edge_index];
    const EditorVertex& v0 = em.vertices[ee.v0];
    const EditorVertex& v1 = em.vertices[ee.v1];

    if ((!ctrl && !shift) || !v0.selected || !v1.selected)
        SelectEdge(edge_index, true);
    else
        SelectEdge(edge_index, false);

    return true;
}

static bool HandleSelectFace()
{
    assert(g_mesh_view.mode == MESH_EDITOR_MODE_FACE);

    EditorAsset& ea = GetEditingAsset();
    EditorMesh& em = GetEditingMesh();
    int triangle_index = HitTestFace(
        em,
        ea.position,
        ScreenToWorld(g_view.camera, GetMousePosition()),
        nullptr);

    if (triangle_index == -1)
        return false;

    bool ctrl = IsCtrlDown(g_view.input);
    bool shift = IsShiftDown(g_view.input);

    if (!ctrl && !shift)
        ClearSelection();

    EditorFace& et = em.faces[triangle_index];
    const EditorVertex& v0 = em.vertices[et.v0];
    const EditorVertex& v1 = em.vertices[et.v1];
    const EditorVertex& v2 = em.vertices[et.v2];

    if ((!ctrl && !shift) || !v0.selected || !v1.selected || !v2.selected)
        SelectFace(triangle_index, true);
    else
        SelectFace(triangle_index, false);

    return true;
}

static void AddVertexAtMouse()
{
    if (g_mesh_view.state != MESH_EDITOR_STATE_DEFAULT)
        return;

    RecordUndo();

    EditorAsset& ea = GetEditingAsset();
    EditorMesh& em = GetEditingMesh();
    int new_vertex = AddVertex(em, g_view.mouse_world_position - ea.position);
    if (new_vertex == -1)
    {
        CancelUndo();
        return;
    }

    ClearSelection();
    SelectVertex(new_vertex, true);
}

static void MergeVertices()
{
    EditorAsset& ea = GetEditingAsset();
    EditorMesh& em = GetEditingMesh();
    if (em.selected_count < 2)
        return;

    MergeSelectedVerticies(em);
    MarkDirty(em);
    MarkModified(ea);
    UpdateSelection();
}

static void DissolveSelected()
{
    EditorAsset& ea = GetEditingAsset();
    EditorMesh& em = GetEditingMesh();

    if (em.selected_count == 0)
        return;

    RecordUndo();

    switch (g_mesh_view.mode)
    {
    case MESH_EDITOR_MODE_VERTEX:
        DissolveSelectedVertices(em);
        break;

    case MESH_EDITOR_MODE_FACE:
        DissolveSelectedFaces(em);
        break;
    }

    MarkDirty(em);
    MarkModified(ea);
    UpdateSelection();
}

static void RotateEdges()
{
    if (g_mesh_view.mode != MESH_EDITOR_MODE_EDGE)
        return;

    EditorAsset& ea = GetEditingAsset();
    EditorMesh& em = GetEditingMesh();

    if (em.selected_count != 1)
        return;

    int edge_index = GetFirstSelectedEdge();
    assert(edge_index != -1);

    RecordUndo();
    edge_index = RotateEdge(em, edge_index);
    if (edge_index == -1)
    {
        CancelUndo();
        return;
    }

    MarkDirty(em);
    ClearSelection();
    SelectEdge(edge_index, true);
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
    if (!g_view.drag && WasButtonReleased(g_view.input, MOUSE_LEFT))
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

    EditorAsset& ea = *(EditorAsset*)input.user_data;
    if (IsCtrlDown(g_view.input))
        SetEdgeColor(GetEditingMesh(), { col, row });
    else
        SetSelectedTrianglesColor(GetEditingMesh(), { col, row });

    MarkModified(ea);
    return true;
}

void MeshViewUpdate()
{
    EditorAsset& ea = GetEditingAsset();

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
    EditorAsset& ea = GetEditingAsset();
    EditorMesh& em = GetEditingMesh();

    for (int i=0; i<em.vertex_count; i++)
    {
        EditorVertex& ev = em.vertices[i];
        if (!ev.selected || !IsVertexOnOutsideEdge(em, i))
            continue;

        BindColor(COLOR_VERTEX_SELECTED);
        DrawMesh(g_view.circle_mesh, TRS(ev.position + ea.position, 0, VEC2_ONE * CIRCLE_CONTROL_OUTLINE_SIZE * g_view.zoom_ref_scale));
    }

    for (int i=0; i<em.vertex_count; i++)
    {
        EditorVertex& ev = em.vertices[i];
        if (!ev.selected || !IsVertexOnOutsideEdge(em, i))
            continue;

        f32 h = value_func(ev);
        i32 arc = Clamp((i32)(100 * h), 0, 100);

        BindColor(COLOR_BLACK);
        DrawMesh(g_view.circle_mesh, TRS(ev.position + ea.position, 0, VEC2_ONE * CIRCLE_CONTROL_SIZE * g_view.zoom_ref_scale));
        BindColor(COLOR_VERTEX_SELECTED);
        DrawMesh(g_view.arc_mesh[arc], TRS(ev.position + ea.position, 0, VEC2_ONE * CIRCLE_CONTROL_SIZE * g_view.zoom_ref_scale));
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
    EditorAsset& ea = GetEditingAsset();
    EditorMesh& em = GetEditingMesh();

    // Mesh
    BindColor(COLOR_WHITE);
    DrawMesh(em, Translate(ea.position));

    // Edges
    BindColor(COLOR_EDGE);
    DrawEdges(em, ea.position);

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
        DrawSelectedEdges(em, ea.position);
        break;

    case MESH_EDITOR_MODE_FACE:
        BindColor(COLOR_VERTEX_SELECTED);
        DrawSelectedFaces(em, ea.position);
        DrawFaceCenters(em, ea.position);
        break;
    }

    // Tools
    switch (g_mesh_view.state)
    {
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
    EditorMesh& em = GetEditingMesh();
    Bounds2 bounds = BOUNDS2_ZERO;
    bool first = true;
    for (int i = 0; i < em.vertex_count; i++)
    {
        const EditorVertex& ev = em.vertices[i];
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
    EditorAsset& ea = GetEditingAsset();
    EditorMesh& em = GetEditingMesh();

    bool shift = IsShiftDown(g_view.input);
    bool ctrl = IsCtrlDown(g_view.input);

    if (!shift && !ctrl)
        ClearSelection();

    switch (g_mesh_view.mode)
    {
    case MESH_EDITOR_MODE_VERTEX:
        for (int i=0; i<em.vertex_count; i++)
        {
            EditorVertex& ev = em.vertices[i];
            Vec2 vpos = ev.position + ea.position;

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
        for (int edge_index=0; edge_index<em.edge_count; edge_index++)
        {
            EditorEdge& ee = em.edges[edge_index];
            Vec2 ev0 = em.vertices[ee.v0].position + ea.position;
            Vec2 ev1 = em.vertices[ee.v1].position + ea.position;
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
        for (int face_index=0; face_index<em.face_count; face_index++)
        {
            EditorFace& ef = em.faces[face_index];
            Vec2 ev0 = em.vertices[ef.v0].position + ea.position;
            Vec2 ev1 = em.vertices[ef.v1].position + ea.position;
            Vec2 ev2 = em.vertices[ef.v2].position + ea.position;
            if (Intersects(bounds, ev0, ev1, ev2))
            {
                if (!ctrl)
                    SelectFace(face_index, true);
                else
                    SelectFace(face_index, false);
            }
        }
        break;

    default:
        break;
    }
}

static void HandleMoveCommand()
{
    if (g_mesh_view.state != MESH_EDITOR_STATE_DEFAULT)
        return;

    EditorMesh& em = GetEditingMesh();
    if (em.selected_count == 0)
        return;

    SetState(MESH_EDITOR_STATE_MOVE);
}

static void HandleScaleCommand()
{
    if (g_mesh_view.state != MESH_EDITOR_STATE_DEFAULT)
        return;

    EditorMesh& em = GetEditingMesh();
    if (em.selected_count == 0)
        return;

    SetState(MESH_EDITOR_STATE_SCALE);
}

static void HandleNormalCommand()
{
    if (g_mesh_view.state != MESH_EDITOR_STATE_DEFAULT)
        return;

    EditorMesh& em = GetEditingMesh();
    if (em.selected_count == 0)
        return;



    SetState(MESH_EDITOR_STATE_NORMAL);
}

static void HandleEdgeCommand()
{
    if (g_mesh_view.state != MESH_EDITOR_STATE_DEFAULT)
        return;

    EditorMesh& em = GetEditingMesh();
    if (em.selected_count == 0)
        return;

    bool has_outside_edge = false;
    for (int i=0; i<em.vertex_count && !has_outside_edge; i++)
    {
        EditorVertex& ev = em.vertices[i];
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
    g_mesh_view.use_fixed_value = text_input->length > 0;
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
}

static bool MeshViewAllowTextInput()
{
    return
        g_mesh_view.state == MESH_EDITOR_STATE_NORMAL ||
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

static void ExtrudeSelected()
{
    EditorMesh& em = GetEditingMesh();

    if (g_mesh_view.mode != MESH_EDITOR_MODE_EDGE || em.selected_count != 1)
        return;

    RecordUndo();
    int new_edge_index = ExtrudeEdge(em, GetFirstSelectedEdge());
    if (new_edge_index == -1)
    {
        CancelUndo();
        return;
    }

    ClearSelection();
    SelectEdge(new_edge_index, true);
    SetState(MESH_EDITOR_STATE_MOVE);
}

void MeshViewInit()
{
    EditorMesh& em = GetEditingMesh();

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

    for (int i=0; i<em.vertex_count; i++)
        em.vertices[i].selected = false;

    if (!g_mesh_view.color_material)
    {
        g_mesh_view.color_material = CreateMaterial(ALLOCATOR_DEFAULT, SHADER_UI);
        SetTexture(g_mesh_view.color_material, TEXTURE_PALETTE, 0);
    }

    if (!g_mesh_view.shortcuts)
    {
        static Shortcut shortcuts[] = {
            { KEY_G, false, false, false, HandleMoveCommand },
            { KEY_S, false, false, false, HandleScaleCommand },
            { KEY_Q, false, false, false, HandleNormalCommand },
            { KEY_W, false, false, false, HandleEdgeCommand },
            { KEY_A, false, false, false, HandleSelectAllCommand },
            { KEY_X, false, false, false, DissolveSelected },
            { KEY_V, false, false, false, AddVertexAtMouse },
            { KEY_M, false, false, false, MergeVertices },
            { KEY_R, false, false, false, RotateEdges },
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

