//
//  MeshZ - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"
#include "tui/text_input.h"

extern int HitTestVertex(const EditableMesh& em, const Vec2& world_pos, float dist);
extern Vec2 SnapToGrid(const Vec2& position, bool secondary);

constexpr float HEIGHT_MIN = -5.0f;
constexpr float HEIGHT_MAX = 5.0f;
constexpr float HEIGHT_SLIDER_SIZE = 2.0f;
constexpr float VERTEX_SIZE = 0.08f;
constexpr float VERTEX_HIT_SIZE = VERTEX_SIZE * 5.0f;
constexpr float CENTER_SIZE = 0.2f;
constexpr float ORIGIN_SIZE = 0.1f;
constexpr float ORIGIN_BORDER_SIZE = 0.12f;
constexpr float SCALE_TOOL_WIDTH = 0.02f;
constexpr Color COLOR_EDGE = { 0,0,0, 0.5f };
constexpr Color COLOR_VERTEX = { 0,0,0,1 };
constexpr Color COLOR_CENTER = { 1, 1, 1, 0.5f};
constexpr Color COLOR_ORIGIN = { 1.0f, 159.0f / 255.0f, 44.0f / 255.0f, 1};
constexpr Color COLOR_ORIGIN_BORDER = { 0,0,0,1 };

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
};

static MeshEditor g_mesh_editor = {};

static void DrawVertices(const EditableAsset& ea, bool selected)
{
    const EditableMesh& em = *ea.mesh;
    for (int i=0; i<em.vertex_count; i++)
    {
        const EditableVertex& ev = em.vertices[i];
        if (ev.selected != selected)
            continue;
        BindTransform(TRS(ev.position + ea.position, 0, VEC2_ONE * g_asset_editor.zoom_ref_scale * VERTEX_SIZE));
        DrawMesh(g_asset_editor.vertex_mesh);
    }
}

static void UpdateSelection(EditableAsset& ea)
{
    EditableMesh& em = *ea.mesh;
    Vec2 center = VEC2_ZERO;
    int count = 0;
    for (int i=0; i<em.vertex_count; i++)
    {
        const EditableVertex& ev = em.vertices[i];
        if (!ev.selected)
            continue;

        center += ev.position;
        count++;
    }

    if (count > 0)
        center = center * (1.0f / count);

    g_mesh_editor.selection_center = center;
}

static void RevertPositions(EditableAsset& ea)
{
    EditableMesh& em = *ea.mesh;
    for (int i=0; i<em.vertex_count; i++)
    {
        EditableVertex& ev = em.vertices[i];
        ev.position = ev.saved_position;
    }

    MarkDirty(em);
    MarkModified(em);
    UpdateSelection(ea);
}

static void UpdateHeightState(EditableAsset& ea)
{
    float delta = (g_asset_editor.mouse_position.y - g_mesh_editor.state_mouse.y) / (g_asset_editor.dpi * HEIGHT_SLIDER_SIZE);

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

    EditableMesh& em = *ea.mesh;
    for (i32 i=0; i<em.vertex_count; i++)
    {
        EditableVertex& ev = em.vertices[i];
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

static void UpdateScaleState(EditableAsset& ea)
{
    float delta =
        Length(g_asset_editor.mouse_world_position - g_mesh_editor.selection_drag_start) -
        Length(g_mesh_editor.world_drag_start - g_mesh_editor.selection_drag_start);

    EditableMesh& em = *ea.mesh;
    for (i32 i=0; i<em.vertex_count; i++)
    {
        EditableVertex& ev = em.vertices[i];
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

static void UpdateMoveState(EditableAsset& ea)
{
    Vec2 delta = g_asset_editor.mouse_world_position - g_mesh_editor.world_drag_start;
    Vec2 new_center = g_mesh_editor.selection_drag_start + delta;
    if (IsButtonDown(g_asset_editor.input, KEY_LEFT_CTRL))
        new_center = SnapToGrid(new_center, true);

    EditableMesh& em = *ea.mesh;
    for (int i=0; i<em.vertex_count; i++)
    {
        EditableVertex& ev = em.vertices[i];
        if (ev.selected)
            ev.position = ev.saved_position + delta;
    }

    MarkDirty(em);
    MarkModified(em);
}

static void SetState(EditableAsset& ea, MeshEditorState state)
{
    g_mesh_editor.state = state;
    g_mesh_editor.world_drag_start = g_asset_editor.mouse_world_position;
    g_mesh_editor.state_mouse = g_asset_editor.mouse_position;
    g_mesh_editor.selection_drag_start = ea.position + g_mesh_editor.selection_center;
    g_mesh_editor.use_fixed_value = false;
    g_mesh_editor.use_negative_fixed_value = false;

    for (int i=0; i<ea.mesh->vertex_count; i++)
    {
        EditableVertex& ev = ea.mesh->vertices[i];
        ev.saved_position = ev.position;
        ev.saved_height = ev.height;
    }
}

static bool SelectVertex(EditableAsset& ea)
{
    EditableMesh& em = *ea.mesh;
    int vertex_index = HitTestVertex(
        em,
        ScreenToWorld(g_asset_editor.camera, GetMousePosition()) - ea.position,
        VERTEX_HIT_SIZE * g_asset_editor.zoom_ref_scale);

    if (vertex_index == -1)
        return false;

    if (!IsButtonDown(g_asset_editor.input, KEY_LEFT_CTRL) && !IsButtonDown(g_asset_editor.input, KEY_LEFT_SHIFT))
        SetSelection(em, vertex_index);
    else
        ToggleSelection(em, vertex_index);

    UpdateSelection(ea);

    return true;
}

static bool SelectEdge(EditableAsset& ea)
{
    EditableMesh& em = *ea.mesh;
    int edge_index = HitTestEdge(
        em,
        ScreenToWorld(g_asset_editor.camera, GetMousePosition()) - ea.position,
        VERTEX_HIT_SIZE * g_asset_editor.zoom_ref_scale);

    if (edge_index == -1)
        return false;

    if (!IsButtonDown(g_asset_editor.input, KEY_LEFT_CTRL) && !IsButtonDown(g_asset_editor.input, KEY_LEFT_SHIFT))
        ClearSelection(em);

    EditableEdge& ee = em.edges[edge_index];
    AddSelection(em, ee.v0);
    AddSelection(em, ee.v1);
    UpdateSelection(ea);

    return true;
}

static bool SelectTriangle(EditableAsset& ea)
{
    EditableMesh& em = *ea.mesh;
    int triangle_index = HitTestTriangle(
        em,
        ea.position,
        ScreenToWorld(g_asset_editor.camera, GetMousePosition()),
        nullptr);

    if (triangle_index == -1)
        return false;

    if (!IsButtonDown(g_asset_editor.input, KEY_LEFT_CTRL) && !IsButtonDown(g_asset_editor.input, KEY_LEFT_SHIFT))
        ClearSelection(em);

    EditableTriangle& et = em.triangles[triangle_index];
    AddSelection(em, et.v0);
    AddSelection(em, et.v1);
    AddSelection(em, et.v2);

    UpdateSelection(ea);

    return true;
}

static bool AddVertexAtMouse(EditableAsset& ea)
{
    EditableMesh& em = *ea.mesh;
    int new_vertex = AddVertex(em, g_asset_editor.mouse_world_position - ea.position);
    if (new_vertex != -1)
    {
        SetSelection(em, new_vertex);
        UpdateSelection(ea);
        return true;
    }
    
    return false;
}

static void MergeVertices(EditableAsset& ea)
{
    EditableMesh& em = *ea.mesh;
    MergeSelectedVerticies(em);
    MarkDirty(em);
    MarkModified(em);
    UpdateSelection(ea);
}

static void DissolveSelected(EditableAsset& ea)
{
    EditableMesh& em = *ea.mesh;
    DissolveSelectedVertices(em);
    MarkDirty(em);
    MarkModified(em);
    UpdateSelection(ea);
}

static bool RotateEdges(EditableAsset& ea)
{
    EditableMesh& em = *ea.mesh;
    int count = 0;
    for (int i=0; i<em.edge_count; i++)
    {
        EditableEdge& ee = em.edges[i];
        if (!em.vertices[ee.v0].selected || !em.vertices[ee.v1].selected)
            continue;

        RotateEdge(em, i);
        count++;
    }

    if (count == 0)
        return false;

    MarkDirty(em);
    MarkModified(em);
    UpdateSelection(ea);

    return true;
}

static void UpdateDefaultState(EditableAsset& ea)
{
    EditableMesh& em = *ea.mesh;

    if (g_asset_editor.drag)
    {
        PushState(ASSET_EDITOR_STATE_BOX_SELECT);
        return;
    }

    // Select
    if (WasButtonPressed(g_asset_editor.input, MOUSE_LEFT))
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

    // Select All
    if (WasButtonPressed(g_asset_editor.input, KEY_A))
    {
        SelectAll(em);
        return;
    }

    // Dissolve
    if (WasButtonPressed(g_asset_editor.input, KEY_X))
    {
        DissolveSelected(ea);
        return;
    }

    // Merge vertices
    if (WasButtonPressed(g_asset_editor.input, KEY_M) && em.selected_vertex_count > 1)
    {
        MergeVertices(ea);
        return;
    }

    // Add
    if (WasButtonPressed(g_asset_editor.input, KEY_V))
    {
        if (AddVertexAtMouse(ea))
            return;
    }

    // Rotate edge
    if (WasButtonPressed(g_asset_editor.input, KEY_R))
    {
        if (RotateEdges(ea))
            return;
    }

    // Enter move state?
    if (g_mesh_editor.state == MESH_EDITOR_STATE_DEFAULT &&
        WasButtonPressed(g_asset_editor.input, KEY_G) &&
        em.selected_vertex_count > 0)
    {
        SetState(ea, MESH_EDITOR_STATE_MOVE);
        return;
    }

    // Enter scale state?
    if (g_mesh_editor.state == MESH_EDITOR_STATE_DEFAULT &&
        !IsButtonDown(g_asset_editor.input, KEY_LEFT_CTRL) &&
        WasButtonPressed(g_asset_editor.input, KEY_S) &&
        em.selected_vertex_count > 0)
    {
        SetState(ea, MESH_EDITOR_STATE_SCALE);
        return;
    }

    if (g_mesh_editor.state == MESH_EDITOR_STATE_DEFAULT &&
        IsButtonDown(g_asset_editor.input, KEY_Q) &&
        em.selected_vertex_count > 0)
    {
        SetState(ea, MESH_EDITOR_STATE_HEIGHT);
        return;
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

    EditableAsset& ea = *(EditableAsset*)input.user_data;
    SetSelectedTrianglesColor(*ea.mesh, { col, row });
    return true;
}

void UpdateMeshEditor(EditableAsset& ea)
{
    BeginCanvas();
    SetStyleSheet(g_assets.ui.mesh_editor);
    Image(g_mesh_editor.color_material, g_names.color_picker_image);
    SetInputHandler(HandleColorPickerInput, &ea);
    EndCanvas();

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
        RevertPositions(ea);
        g_mesh_editor.state = MESH_EDITOR_STATE_DEFAULT;
    }
}

static void DrawScaleState(EditableAsset& ea)
{
    BindColor(SetAlpha(COLOR_CENTER, 0.75f));
    DrawVertex(g_mesh_editor.selection_drag_start, CENTER_SIZE * 0.75f);
    BindColor(COLOR_CENTER);
    DrawLine(g_asset_editor.mouse_world_position, g_mesh_editor.selection_drag_start, SCALE_TOOL_WIDTH);
    BindColor(COLOR_ORIGIN);
    DrawVertex(g_asset_editor.mouse_world_position, CENTER_SIZE);
}

static void DrawHeightState(EditableAsset& ea)
{
    Vec2 h1 =
        ScreenToWorld(g_asset_editor.camera, {0, g_asset_editor.dpi * HEIGHT_SLIDER_SIZE}) -
        ScreenToWorld(g_asset_editor.camera, VEC2_ZERO);

    float total_height = 0.0f;
    int height_count = 0;
    for (int i=0; i<ea.mesh->vertex_count; i++)
    {
        EditableVertex& ev = ea.mesh->vertices[i];
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

void DrawMeshEditor(EditableAsset& ea)
{
    DrawEdges(ea, 10000, COLOR_EDGE);
    BindColor(COLOR_VERTEX);
    DrawVertices(ea, false);

    BindColor(COLOR_SELECTED);
    DrawVertices(ea, true);

    BindColor(COLOR_ORIGIN_BORDER);
    DrawVertex(ea.position, ORIGIN_BORDER_SIZE);
    BindColor(COLOR_ORIGIN);
    DrawVertex(ea.position, ORIGIN_SIZE);

    BindTransform(TRS(ea.position, 0, VEC2_ONE * g_asset_editor.zoom_ref_scale * ORIGIN_SIZE));
    DrawMesh(g_asset_editor.vertex_mesh);

    switch (g_mesh_editor.state)
    {
    case MESH_EDITOR_STATE_SCALE:
        DrawScaleState(ea);
        break;

    case MESH_EDITOR_STATE_HEIGHT:
        DrawHeightState(ea);
        break;

    default:
        break;
    }
}

void HandleMeshEditorBoxSelect(EditableAsset& ea, const Bounds2& bounds)
{
    EditableMesh& em = *ea.mesh;
    ClearSelection(em);
    for (int i=0; i<em.vertex_count; i++)
    {
        EditableVertex& ev = em.vertices[i];
        Vec2 vpos = ev.position + ea.position;
        if (vpos.x >= bounds.min.x && vpos.x <= bounds.max.x &&
            vpos.y >= bounds.min.y && vpos.y <= bounds.max.y)
            AddSelection(em, i);
    }

    UpdateSelection(ea);
}

void InitMeshEditor(EditableAsset& ea)
{
    g_mesh_editor.state = MESH_EDITOR_STATE_DEFAULT;

    for (int i=0; i<ea.mesh->vertex_count; i++)
        ea.mesh->vertices[i].selected = false;

    if (!g_mesh_editor.color_material)
    {
        g_mesh_editor.color_material = CreateMaterial(ALLOCATOR_DEFAULT, g_core_assets.shaders.ui);
        SetTexture(g_mesh_editor.color_material, g_assets.textures.palette, 0);
    }
}

