//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "editor_assets.h"

#include <editor.h>

extern Vec2 SnapToGrid(const Vec2& position, bool secondary);

constexpr float HEIGHT_MIN = -1.0f;
constexpr float HEIGHT_MAX = 1.0f;
constexpr float HEIGHT_SLIDER_SIZE = 2.0f;
constexpr float VERTEX_SIZE = 0.08f;
constexpr float VERTEX_HIT_SIZE = 20.0f;
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
    MESH_EDITOR_STATE_HEIGHT,
};

struct MeshEditor
{
    MeshEditorState state;
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
};

static MeshEditor g_mesh_editor = {};

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
    int count = 0;
    for (int i=0; i<em.vertex_count; i++)
    {
        const EditorVertex& ev = em.vertices[i];
        if (!ev.selected)
            continue;

        center += ev.position;
        count++;
    }

    if (count > 0)
        center = center * (1.0f / count);

    g_mesh_editor.selection_center = center;
}

static void RevertPositions()
{
    EditorAsset& ea = GetEditingAsset();
    EditorMesh& em = GetEditingMesh();
    for (int i=0; i<em.vertex_count; i++)
    {
        EditorVertex& ev = em.vertices[i];
        ev.position = ev.saved_position;
    }

    MarkDirty(em);
    MarkModified(ea);
    UpdateSelection();
}

static void UpdateHeightState()
{
    EditorAsset& ea = GetEditingAsset();
    EditorMesh& em = GetEditingMesh();
    float delta = (g_view.mouse_position.y - g_mesh_editor.state_mouse.y) / (g_view.dpi * HEIGHT_SLIDER_SIZE);

    if (WasButtonPressed(g_view.input, KEY_ESCAPE))
    {
        for (i32 i=0; i<em.vertex_count; i++)
        {
            EditorVertex& ev = em.vertices[i];
            if (!ev.selected) continue;
            ev.height = ev.saved_height;;
        }

        CancelUndo();
        MarkDirty(em);
        MarkModified(ea);
        return;
    }
    if (WasButtonPressed(g_view.input, KEY_0))
    {
        g_mesh_editor.use_fixed_value = true;
        g_mesh_editor.use_negative_fixed_value = false;
        g_mesh_editor.fixed_value = 0.0f;
    }
    else if (WasButtonPressed(g_view.input, KEY_1))
    {
        g_mesh_editor.use_fixed_value = true;
        if (g_mesh_editor.use_negative_fixed_value)
            g_mesh_editor.fixed_value = HEIGHT_MIN;
        else
            g_mesh_editor.fixed_value = HEIGHT_MAX;
    }
    else if (WasButtonPressed(g_view.input, KEY_MINUS))
        g_mesh_editor.use_negative_fixed_value = true;

    for (i32 i=0; i<em.vertex_count; i++)
    {
        EditorVertex& ev = em.vertices[i];
        if (!ev.selected)
            continue;

        if (g_mesh_editor.use_fixed_value)
            ev.height = g_mesh_editor.fixed_value;
        else
            ev.height = Clamp(ev.saved_height - delta * (HEIGHT_MAX-HEIGHT_MIN) * 0.5f, HEIGHT_MIN, HEIGHT_MAX);
    }

    MarkDirty(em);
    MarkModified(ea);

    if (WasButtonPressed(g_view.input, MOUSE_LEFT) || WasButtonPressed(g_view.input, KEY_ENTER))
    {
        g_mesh_editor.state = MESH_EDITOR_STATE_DEFAULT;
    }
    else if (WasButtonPressed(g_view.input, MOUSE_RIGHT))
    {
        RevertPositions();
        g_mesh_editor.state = MESH_EDITOR_STATE_DEFAULT;
    }
}

static void UpdateScaleState(EditorAsset& ea)
{
    float delta =
        Length(g_view.mouse_world_position - g_mesh_editor.selection_drag_start) -
        Length(g_mesh_editor.world_drag_start - g_mesh_editor.selection_drag_start);

    EditorMesh& em = GetEditingMesh();
    for (i32 i=0; i<em.vertex_count; i++)
    {
        EditorVertex& ev = em.vertices[i];
        if (!ev.selected)
            continue;

        Vec2 dir = ev.saved_position - g_mesh_editor.selection_center;
        ev.position = g_mesh_editor.selection_center + dir * (1.0f + delta);
    }

    MarkDirty(em);
    MarkModified(ea);

    if (WasButtonPressed(g_view.input, MOUSE_LEFT))
    {
        UpdateSelection();
        g_mesh_editor.state = MESH_EDITOR_STATE_DEFAULT;
    }
    else if (WasButtonPressed(g_view.input, MOUSE_RIGHT))
    {
        RevertPositions();
        g_mesh_editor.state = MESH_EDITOR_STATE_DEFAULT;
    }
}

static void UpdateMoveState()
{
    Vec2 delta = g_view.mouse_world_position - g_mesh_editor.world_drag_start;

    EditorMesh& em = GetEditingMesh();
    for (int i=0; i<em.vertex_count; i++)
    {
        EditorVertex& ev = em.vertices[i];
        if (ev.selected)
            ev.position = ev.saved_position + delta;
    }

    MarkDirty(em);
    MarkModified(GetEditingAsset());
}

static void SetState(MeshEditorState state)
{
    EditorAsset& ea = GetEditingAsset();
    g_mesh_editor.state = state;
    g_mesh_editor.world_drag_start = g_view.mouse_world_position;
    g_mesh_editor.state_mouse = g_view.mouse_position;
    g_mesh_editor.selection_drag_start = ea.position + g_mesh_editor.selection_center;
    g_mesh_editor.use_fixed_value = false;
    g_mesh_editor.use_negative_fixed_value = false;

    EditorMesh& em = GetEditingMesh();
    for (int i=0; i<em.vertex_count; i++)
    {
        EditorVertex& ev = em.vertices[i];
        ev.saved_position = ev.position;
        ev.saved_height = ev.height;
    }

    switch (state)
    {
    case MESH_EDITOR_STATE_MOVE:
        RecordUndo(ea);
        break;

    case MESH_EDITOR_STATE_SCALE:
        RecordUndo(ea);
        break;

    case MESH_EDITOR_STATE_HEIGHT:
        RecordUndo(ea);
        break;

    default:
        break;
    }
}

static bool SelectVertex(EditorAsset& ea)
{
    EditorMesh& em = GetEditingMesh();
    int vertex_index = HitTestVertex(em, g_view.mouse_world_position - ea.position);
    if (vertex_index == -1)
        return false;

    if (IsCtrlDown(g_view.input) || IsShiftDown(g_view.input))
        ToggleSelection(em, vertex_index);
    else
        SetSelection(em, vertex_index);

    UpdateSelection();

    return true;
}

static bool SelectEdge(EditorAsset& ea)
{
    EditorMesh& em = GetEditingMesh();
    int edge_index = HitTestEdge(em, g_view.mouse_world_position - ea.position);
    if (edge_index == -1)
        return false;

    bool ctrl = IsCtrlDown(g_view.input);
    bool shift = IsShiftDown(g_view.input);

    if (!ctrl && !shift)
        ClearSelection(em);

    EditorEdge& ee = em.edges[edge_index];
    const EditorVertex& v0 = em.vertices[ee.v0];
    const EditorVertex& v1 = em.vertices[ee.v1];

    if ((!ctrl && !shift) || !v0.selected || !v1.selected)
    {
        AddSelection(em, ee.v0);
        AddSelection(em, ee.v1);
    }
    else
    {
        RemoveSelection(em, ee.v0);
        RemoveSelection(em, ee.v1);
    }

    UpdateSelection();

    return true;
}

static bool SelectTriangle(EditorAsset& ea)
{
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
        ClearSelection(em);

    EditorFace& et = em.faces[triangle_index];
    const EditorVertex& v0 = em.vertices[et.v0];
    const EditorVertex& v1 = em.vertices[et.v1];
    const EditorVertex& v2 = em.vertices[et.v2];

    if ((!ctrl && !shift) || !v0.selected || !v1.selected || !v2.selected)
    {
        AddSelection(em, et.v0);
        AddSelection(em, et.v1);
        AddSelection(em, et.v2);
    }
    else
    {
        RemoveSelection(em, et.v0);
        RemoveSelection(em, et.v1);
        RemoveSelection(em, et.v2);
    }

    UpdateSelection();

    return true;
}

static void AddVertexAtMouse()
{
    if (g_mesh_editor.state != MESH_EDITOR_STATE_DEFAULT)
        return;

    EditorAsset& ea = GetEditingAsset();
    EditorMesh& em = GetEditingMesh();
    int new_vertex = AddVertex(em, g_view.mouse_world_position - ea.position);
    if (new_vertex == -1)
        return;

    SetSelection(em, new_vertex);
    UpdateSelection();
}

static void MergeVertices()
{
    EditorAsset& ea = GetEditingAsset();
    EditorMesh& em = GetEditingMesh();
    if (em.selected_vertex_count < 2)
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
    DissolveSelectedVertices(em);
    MarkDirty(em);
    MarkModified(ea);
    UpdateSelection();
}

static void RotateEdges()
{
    EditorAsset& ea = GetEditingAsset();
    EditorMesh& em = GetEditingMesh();
    int edge_index = -1;
    for (int i=0; i<em.edge_count; i++)
    {
        EditorEdge& ee = em.edges[i];
        if (em.vertices[ee.v0].selected && em.vertices[ee.v1].selected)
        {
            edge_index = i;
            break;
        }
    }

    if (edge_index == -1)
        return;

    edge_index = RotateEdge(em, edge_index);
    if (edge_index == -1)
        return;

    MarkDirty(em);
    ClearSelection(em);
    AddSelection(em, em.edges[edge_index].v0);
    AddSelection(em, em.edges[edge_index].v1);
    MarkModified(ea);
    UpdateSelection();
}

static void UpdateDefaultState()
{
    EditorAsset& ea = GetEditingAsset();
    EditorMesh& em = GetEditingMesh();

    // If a drag has started then switch to box select
    if (g_view.drag)
    {
        BeginBoxSelect(HandleBoxSelect);
        return;
    }

    // Select
    if (!g_view.drag && WasButtonReleased(g_view.input, MOUSE_LEFT))
    {
        g_mesh_editor.clear_selection_on_up = false;

        if (SelectVertex(ea))
            return;

        if (SelectEdge(ea))
            return;

        if (SelectTriangle(ea))
            return;

        g_mesh_editor.clear_selection_on_up = true;
    }

    if (WasButtonReleased(g_view.input, MOUSE_LEFT) && g_mesh_editor.clear_selection_on_up)
    {
        ClearSelection(em);
        UpdateSelection();
    }
}

bool HandleColorPickerInput(const ElementInput& input)
{
    float x = (input.mouse_position.x - GetLeft(input.bounds)) / input.bounds.width;
    float y = (input.mouse_position.y - GetTop(input.bounds)) / input.bounds.height;
    if (x < 0 || x > 1 || y < 0 || y > 1)
        return false;

    i32 col = (i32)(x * 16.0f);
    i32 row = (i32)(y * 16.0f);

    EditorAsset& ea = *(EditorAsset*)input.user_data;
    SetSelectedTrianglesColor(GetEditingMesh(), { col, row });
    MarkModified(ea);
    return true;
}

void MeshViewUpdate()
{
    EditorAsset& ea = GetEditingAsset();

    BeginCanvas();
    Image(g_mesh_editor.color_material, STYLE_MESH_EDITOR_COLORS);
    SetInputHandler(HandleColorPickerInput, &ea);
    EndCanvas();

    CheckShortcuts(g_mesh_editor.shortcuts);

    switch (g_mesh_editor.state)
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

    case MESH_EDITOR_STATE_HEIGHT:
        UpdateHeightState();
        break;

    default:
        break;
    }

    // Commit the tool
    if (WasButtonPressed(g_view.input, MOUSE_LEFT) || WasButtonPressed(g_view.input, KEY_ENTER))
    {
        UpdateSelection();
        g_mesh_editor.state = MESH_EDITOR_STATE_DEFAULT;
    }
    // Cancel the tool
    else if (WasButtonPressed(g_view.input, KEY_ESCAPE) || WasButtonPressed(g_view.input, MOUSE_RIGHT))
    {
        CancelUndo();
        RevertPositions();
        g_mesh_editor.state = MESH_EDITOR_STATE_DEFAULT;
    }
}

static void DrawScaleState()
{
    BindColor(SetAlpha(COLOR_CENTER, 0.75f));
    DrawVertex(g_mesh_editor.selection_drag_start, CENTER_SIZE * 0.75f);
    BindColor(COLOR_CENTER);
    DrawLine(g_view.mouse_world_position, g_mesh_editor.selection_drag_start, ROTATE_TOOL_WIDTH);
    BindColor(COLOR_ORIGIN);
    DrawVertex(g_view.mouse_world_position, CENTER_SIZE);
}

static void DrawHeightState()
{
    EditorMesh& em = GetEditingMesh();
    Vec2 h1 =
        ScreenToWorld(g_view.camera, {0, g_view.dpi * HEIGHT_SLIDER_SIZE}) -
        ScreenToWorld(g_view.camera, VEC2_ZERO);

    float total_height = 0.0f;
    int height_count = 0;
    for (int i=0; i<em.vertex_count; i++)
    {
        EditorVertex& ev = em.vertices[i];
        if (!ev.selected)
            continue;

        total_height += ev.height;
        height_count++;
    }

    float avg_height = total_height / (f32)Max(1, height_count);
    float height_ratio = (avg_height - HEIGHT_MIN) / (HEIGHT_MAX - HEIGHT_MIN);

    BindColor(SetAlpha(COLOR_CENTER, 0.5f));
    DrawVertex(g_mesh_editor.world_drag_start, CENTER_SIZE * 0.75f);
    BindColor(COLOR_CENTER);
    DrawLine(g_mesh_editor.world_drag_start + h1, g_mesh_editor.world_drag_start - h1, ROTATE_TOOL_WIDTH);
    BindColor(COLOR_ORIGIN);
    DrawVertex(g_mesh_editor.world_drag_start + Mix(h1, -h1, height_ratio), CENTER_SIZE);
}

void MeshViewDraw()
{
    EditorAsset& ea = GetEditingAsset();
    DrawEdges(ea, 10000, COLOR_EDGE);
    BindColor(COLOR_VERTEX);
    DrawVertices(false);

    BindColor(COLOR_SELECTED);
    DrawVertices(true);

    DrawOrigin(ea);

    BindTransform(TRS(ea.position, 0, VEC2_ONE * g_view.zoom_ref_scale * ORIGIN_SIZE));
    DrawMesh(g_view.vertex_mesh);

    switch (g_mesh_editor.state)
    {
    case MESH_EDITOR_STATE_SCALE:
        DrawScaleState();
        break;

    case MESH_EDITOR_STATE_HEIGHT:
        DrawHeightState();
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

    return bounds;
}

void HandleBoxSelect(const Bounds2& bounds)
{
    EditorAsset& ea = GetEditingAsset();
    EditorMesh& em = GetEditingMesh();

    bool shift = IsShiftDown(g_view.input);
    bool ctrl = IsCtrlDown(g_view.input);

    if (!shift && !ctrl)
        ClearSelection(em);

    for (int i=0; i<em.vertex_count; i++)
    {
        EditorVertex& ev = em.vertices[i];
        Vec2 vpos = ev.position + ea.position;

        if (vpos.x >= bounds.min.x && vpos.x <= bounds.max.x &&
            vpos.y >= bounds.min.y && vpos.y <= bounds.max.y)
        {
            if (!ctrl)
                AddSelection(em, i);
            else
                RemoveSelection(em, i);
        }
    }

    UpdateSelection();
}

static void HandleMoveCommand()
{
    if (g_mesh_editor.state != MESH_EDITOR_STATE_DEFAULT)
        return;

    EditorMesh& em = GetEditingMesh();
    if (em.selected_vertex_count == 0)
        return;

    SetState(MESH_EDITOR_STATE_MOVE);
}

static void HandleScaleCommand()
{
    if (g_mesh_editor.state != MESH_EDITOR_STATE_DEFAULT)
        return;

    EditorMesh& em = GetEditingMesh();
    if (em.selected_vertex_count == 0)
        return;

    SetState(MESH_EDITOR_STATE_SCALE);
}

static void HandleHeightCommand()
{
    if (g_mesh_editor.state != MESH_EDITOR_STATE_DEFAULT)
        return;

    EditorMesh& em = GetEditingMesh();
    if (em.selected_vertex_count == 0)
        return;

    SetState(MESH_EDITOR_STATE_HEIGHT);
}

static void HandleSelectAllCommand()
{
    SelectAll(GetEditingMesh());
}

void MeshViewInit()
{
    EditorMesh& em = GetEditingMesh();

    g_view.vtable = {
        .update = MeshViewUpdate,
        .draw = MeshViewDraw,
        .bounds = MeshViewBounds,
    };

    g_mesh_editor.state = MESH_EDITOR_STATE_DEFAULT;

    for (int i=0; i<em.vertex_count; i++)
        em.vertices[i].selected = false;

    if (!g_mesh_editor.color_material)
    {
        g_mesh_editor.color_material = CreateMaterial(ALLOCATOR_DEFAULT, SHADER_UI);
        SetTexture(g_mesh_editor.color_material, TEXTURE_PALETTE, 0);
    }

    if (!g_mesh_editor.shortcuts)
    {
        static Shortcut shortcuts[] = {
            { KEY_G, false, false, false, HandleMoveCommand },
            { KEY_S, false, false, false, HandleScaleCommand },
            { KEY_Q, false, false, false, HandleHeightCommand },
            { KEY_A, false, false, false, HandleSelectAllCommand },
            { KEY_X, false, false, false, DissolveSelected },
            { KEY_V, false, false, false, AddVertexAtMouse },
            { KEY_M, false, false, false, MergeVertices },
            { KEY_R, false, false, false, RotateEdges },
            { INPUT_CODE_NONE }
        };

        g_mesh_editor.shortcuts = shortcuts;
        EnableShortcuts(shortcuts);
    }
}

