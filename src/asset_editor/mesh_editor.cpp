//
//  MeshZ - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"

extern Vec2 SnapToGrid(const Vec2& position, bool secondary);

constexpr float HEIGHT_MIN = -5.0f;
constexpr float HEIGHT_MAX = 5.0f;
constexpr float HEIGHT_SLIDER_SIZE = 2.0f;
constexpr float VERTEX_SIZE = 0.08f;
constexpr float VERTEX_HIT_SIZE = 20.0f;
constexpr float CENTER_SIZE = 0.2f;
constexpr float ORIGIN_SIZE = 0.1f;
constexpr float ORIGIN_BORDER_SIZE = 0.12f;
constexpr float SCALE_TOOL_WIDTH = 0.02f;

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
    EditorAsset* asset;
    EditorMesh* mesh;
};

extern Shortcut g_mesh_editor_shortcuts[];
static MeshEditor g_mesh_editor = {};

static void DrawVertices(const EditorAsset& ea, bool selected)
{
    const EditorMesh& em = *ea.mesh;
    for (int i=0; i<em.vertex_count; i++)
    {
        const EditorVertex& ev = em.vertices[i];
        if (ev.selected != selected)
            continue;
        BindTransform(TRS(ev.position + ea.position, 0, VEC2_ONE * g_asset_editor.zoom_ref_scale * VERTEX_SIZE));
        DrawMesh(g_asset_editor.vertex_mesh);
    }
}

static void UpdateSelection(EditorAsset& ea)
{
    EditorMesh& em = *ea.mesh;
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

static void RevertPositions(EditorAsset& ea)
{
    EditorMesh& em = *ea.mesh;
    for (int i=0; i<em.vertex_count; i++)
    {
        EditorVertex& ev = em.vertices[i];
        ev.position = ev.saved_position;
    }

    MarkDirty(em);
    MarkModified(em);
    UpdateSelection(ea);
}

static void UpdateHeightState(EditorAsset& ea)
{
    float delta = (g_asset_editor.mouse_position.y - g_mesh_editor.state_mouse.y) / (g_asset_editor.dpi * HEIGHT_SLIDER_SIZE);

    if (WasButtonPressed(g_asset_editor.input, KEY_ESCAPE))
    {
        EditorMesh& em = *ea.mesh;
        for (i32 i=0; i<em.vertex_count; i++)
        {
            EditorVertex& ev = em.vertices[i];
            if (!ev.selected) continue;
            ev.height = ev.saved_height;;
        }

        CancelUndo();
        MarkDirty(em);
        MarkModified(em);
        return;
    }
    if (WasButtonPressed(g_asset_editor.input, KEY_0))
    {
        g_mesh_editor.use_fixed_value = true;
        g_mesh_editor.use_negative_fixed_value = false;
        g_mesh_editor.fixed_value = 0.0f;
    }
    else if (WasButtonPressed(g_asset_editor.input, KEY_1))
    {
        g_mesh_editor.use_fixed_value = true;
        if (g_mesh_editor.use_negative_fixed_value)
            g_mesh_editor.fixed_value = HEIGHT_MIN;
        else
            g_mesh_editor.fixed_value = HEIGHT_MAX;
    }
    else if (WasButtonPressed(g_asset_editor.input, KEY_MINUS))
        g_mesh_editor.use_negative_fixed_value = true;

    EditorMesh& em = *ea.mesh;
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
    MarkModified(em);

    if (WasButtonPressed(g_asset_editor.input, MOUSE_LEFT) || WasButtonPressed(g_asset_editor.input, KEY_ENTER))
    {
        g_mesh_editor.state = MESH_EDITOR_STATE_DEFAULT;
    }
    else if (WasButtonPressed(g_asset_editor.input, MOUSE_RIGHT))
    {
        RevertPositions(ea);
        g_mesh_editor.state = MESH_EDITOR_STATE_DEFAULT;
    }
}

static void UpdateScaleState(EditorAsset& ea)
{
    float delta =
        Length(g_asset_editor.mouse_world_position - g_mesh_editor.selection_drag_start) -
        Length(g_mesh_editor.world_drag_start - g_mesh_editor.selection_drag_start);

    EditorMesh& em = *ea.mesh;
    for (i32 i=0; i<em.vertex_count; i++)
    {
        EditorVertex& ev = em.vertices[i];
        if (!ev.selected)
            continue;

        Vec2 dir = ev.saved_position - g_mesh_editor.selection_center;
        ev.position = g_mesh_editor.selection_center + dir * (1.0f + delta);
    }

    MarkDirty(em);
    MarkModified(em);

    if (WasButtonPressed(g_asset_editor.input, MOUSE_LEFT))
    {
        UpdateSelection(ea);
        g_mesh_editor.state = MESH_EDITOR_STATE_DEFAULT;
    }
    else if (WasButtonPressed(g_asset_editor.input, MOUSE_RIGHT))
    {
        RevertPositions(ea);
        g_mesh_editor.state = MESH_EDITOR_STATE_DEFAULT;
    }
}

static void UpdateMoveState(EditorAsset& ea)
{
    Vec2 delta = g_asset_editor.mouse_world_position - g_mesh_editor.world_drag_start;
    Vec2 new_center = g_mesh_editor.selection_drag_start + delta;
    if (IsButtonDown(g_asset_editor.input, KEY_LEFT_CTRL))
        new_center = SnapToGrid(new_center, true);

    EditorMesh& em = *ea.mesh;
    for (int i=0; i<em.vertex_count; i++)
    {
        EditorVertex& ev = em.vertices[i];
        if (ev.selected)
            ev.position = ev.saved_position + delta;
    }

    MarkDirty(em);
    MarkModified(em);
}

static void SetState(EditorAsset& ea, MeshEditorState state)
{
    g_mesh_editor.state = state;
    g_mesh_editor.world_drag_start = g_asset_editor.mouse_world_position;
    g_mesh_editor.state_mouse = g_asset_editor.mouse_position;
    g_mesh_editor.selection_drag_start = ea.position + g_mesh_editor.selection_center;
    g_mesh_editor.use_fixed_value = false;
    g_mesh_editor.use_negative_fixed_value = false;

    for (int i=0; i<ea.mesh->vertex_count; i++)
    {
        EditorVertex& ev = ea.mesh->vertices[i];
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
    EditorMesh& em = *ea.mesh;
    int vertex_index = HitTestVertex(
        em,
        ScreenToWorld(g_asset_editor.camera, GetMousePosition()) - ea.position);

    if (vertex_index == -1)
        return false;

    if (IsCtrlDown(g_asset_editor.input) || IsShiftDown(g_asset_editor.input))
        ToggleSelection(em, vertex_index);
    else
        SetSelection(em, vertex_index);

    UpdateSelection(ea);

    return true;
}

static bool SelectEdge(EditorAsset& ea)
{
    EditorMesh& em = *ea.mesh;
    int edge_index = HitTestEdge(em, ScreenToWorld(g_asset_editor.camera, GetMousePosition()) - ea.position);
    if (edge_index == -1)
        return false;

    bool ctrl = IsCtrlDown(g_asset_editor.input);
    bool shift = IsShiftDown(g_asset_editor.input);

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

    UpdateSelection(ea);

    return true;
}

static bool SelectTriangle(EditorAsset& ea)
{
    EditorMesh& em = *ea.mesh;
    int triangle_index = HitTestTriangle(
        em,
        ea.position,
        ScreenToWorld(g_asset_editor.camera, GetMousePosition()),
        nullptr);

    if (triangle_index == -1)
        return false;

    bool ctrl = IsCtrlDown(g_asset_editor.input);
    bool shift = IsShiftDown(g_asset_editor.input);

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

    UpdateSelection(ea);

    return true;
}

static void AddVertexAtMouse()
{
    if (g_mesh_editor.state != MESH_EDITOR_STATE_DEFAULT)
        return;

    EditorAsset& ea = *g_mesh_editor.asset;
    EditorMesh& em = *ea.mesh;
    int new_vertex = AddVertex(em, g_asset_editor.mouse_world_position - ea.position);
    if (new_vertex == -1)
        return;

    SetSelection(em, new_vertex);
    UpdateSelection(ea);
}

static void MergeVertices()
{
    EditorAsset& ea = *g_mesh_editor.asset;
    EditorMesh& em = *ea.mesh;
    if (em.selected_vertex_count < 2)
        return;

    MergeSelectedVerticies(em);
    MarkDirty(em);
    MarkModified(em);
    UpdateSelection(ea);
}

static void DissolveSelected()
{
    EditorMesh& em = *g_mesh_editor.asset->mesh;
    DissolveSelectedVertices(em);
    MarkDirty(em);
    MarkModified(em);
    UpdateSelection(*g_mesh_editor.asset);
}

static void RotateEdges()
{
    EditorAsset& ea = *g_mesh_editor.asset;
    EditorMesh& em = *ea.mesh;
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
    MarkModified(em);
    UpdateSelection(ea);
}

static void UpdateDefaultState(EditorAsset& ea)
{
    EditorMesh& em = *ea.mesh;

    // If a drag has started then switch to box select
    if (g_asset_editor.drag)
    {
        BeginBoxSelect(HandleBoxSelect);
        return;
    }

    // Select
    if (!g_asset_editor.drag && WasButtonReleased(g_asset_editor.input, MOUSE_LEFT))
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

    if (WasButtonReleased(g_asset_editor.input, MOUSE_LEFT) && g_mesh_editor.clear_selection_on_up)
    {
        ClearSelection(em);
        UpdateSelection(ea);
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
    SetSelectedTrianglesColor(*ea.mesh, { col, row });
    return true;
}

void UpdateMeshEditor(EditorAsset& ea)
{
    g_mesh_editor.asset = &ea;
    g_mesh_editor.mesh = ea.mesh;

    BeginCanvas();
    SetStyleSheet(g_assets.ui.mesh_editor);
    Image(g_mesh_editor.color_material, g_names.color_picker_image);
    SetInputHandler(HandleColorPickerInput, &ea);
    EndCanvas();

    CheckShortcuts(g_mesh_editor_shortcuts);

    switch (g_mesh_editor.state)
    {
    case MESH_EDITOR_STATE_DEFAULT:
        UpdateDefaultState(ea);
        return;

    case MESH_EDITOR_STATE_MOVE:
        UpdateMoveState(ea);
        break;

    case MESH_EDITOR_STATE_SCALE:
        UpdateScaleState(ea);
        break;

    case MESH_EDITOR_STATE_HEIGHT:
        UpdateHeightState(ea);
        break;

    default:
        break;
    }

    // Commit the tool
    if (WasButtonPressed(g_asset_editor.input, MOUSE_LEFT) || WasButtonPressed(g_asset_editor.input, KEY_ENTER))
    {
        UpdateSelection(ea);
        g_mesh_editor.state = MESH_EDITOR_STATE_DEFAULT;
    }
    // Cancel the tool
    else if (WasButtonPressed(g_asset_editor.input, KEY_ESCAPE) || WasButtonPressed(g_asset_editor.input, MOUSE_RIGHT))
    {
        CancelUndo();
        RevertPositions(ea);
        g_mesh_editor.state = MESH_EDITOR_STATE_DEFAULT;
    }
}

static void DrawScaleState()
{
    BindColor(SetAlpha(COLOR_CENTER, 0.75f));
    DrawVertex(g_mesh_editor.selection_drag_start, CENTER_SIZE * 0.75f);
    BindColor(COLOR_CENTER);
    DrawLine(g_asset_editor.mouse_world_position, g_mesh_editor.selection_drag_start, SCALE_TOOL_WIDTH);
    BindColor(COLOR_ORIGIN);
    DrawVertex(g_asset_editor.mouse_world_position, CENTER_SIZE);
}

static void DrawHeightState()
{
    EditorAsset& ea = *g_mesh_editor.asset;
    Vec2 h1 =
        ScreenToWorld(g_asset_editor.camera, {0, g_asset_editor.dpi * HEIGHT_SLIDER_SIZE}) -
        ScreenToWorld(g_asset_editor.camera, VEC2_ZERO);

    float total_height = 0.0f;
    int height_count = 0;
    for (int i=0; i<ea.mesh->vertex_count; i++)
    {
        EditorVertex& ev = ea.mesh->vertices[i];
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
    DrawLine(g_mesh_editor.world_drag_start + h1, g_mesh_editor.world_drag_start - h1, SCALE_TOOL_WIDTH);
    BindColor(COLOR_ORIGIN);
    DrawVertex(g_mesh_editor.world_drag_start + Mix(h1, -h1, height_ratio), CENTER_SIZE);
}

void DrawMeshEditor(EditorAsset& ea)
{
    DrawEdges(ea, 10000, COLOR_EDGE);
    BindColor(COLOR_VERTEX);
    DrawVertices(ea, false);

    BindColor(COLOR_SELECTED);
    DrawVertices(ea, true);

    DrawOrigin(ea);

    BindTransform(TRS(ea.position, 0, VEC2_ONE * g_asset_editor.zoom_ref_scale * ORIGIN_SIZE));
    DrawMesh(g_asset_editor.vertex_mesh);

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

static void HandleBoxSelect(const Bounds2& bounds)
{
    EditorAsset& ea = *g_mesh_editor.asset;
    EditorMesh& em = *ea.mesh;

    bool shift = IsShiftDown(g_asset_editor.input);
    bool ctrl = IsCtrlDown(g_asset_editor.input);

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

    UpdateSelection(ea);
}

static void HandleMoveCommand()
{
    if (g_mesh_editor.state != MESH_EDITOR_STATE_DEFAULT)
        return;

    if (g_mesh_editor.mesh->selected_vertex_count == 0)
        return;

    SetState(*g_mesh_editor.asset, MESH_EDITOR_STATE_MOVE);
}

static void HandleScaleCommand()
{
    if (g_mesh_editor.state != MESH_EDITOR_STATE_DEFAULT)
        return;

    if (g_mesh_editor.mesh->selected_vertex_count == 0)
        return;

    SetState(*g_mesh_editor.asset, MESH_EDITOR_STATE_SCALE);
}

static void HandleHeightCommand()
{
    if (g_mesh_editor.state != MESH_EDITOR_STATE_DEFAULT)
        return;

    if (g_mesh_editor.mesh->selected_vertex_count == 0)
        return;

    SetState(*g_mesh_editor.asset, MESH_EDITOR_STATE_HEIGHT);
}

static void HandleSelectAllCommand()
{
    SelectAll(*g_mesh_editor.mesh);
}

static Shortcut g_mesh_editor_shortcuts[] = {
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

void InitMeshEditor(EditorAsset& ea)
{
    g_mesh_editor.state = MESH_EDITOR_STATE_DEFAULT;

    EnableShortcuts(g_mesh_editor_shortcuts);

    for (int i=0; i<ea.mesh->vertex_count; i++)
        ea.mesh->vertices[i].selected = false;

    if (!g_mesh_editor.color_material)
    {
        g_mesh_editor.color_material = CreateMaterial(ALLOCATOR_DEFAULT, g_core_assets.shaders.ui);
        SetTexture(g_mesh_editor.color_material, g_assets.textures.palette, 0);
    }
}

