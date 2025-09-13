//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "view.h"

constexpr int MAX_COMMAND_LENGTH = 1024;
constexpr float SELECT_SIZE = 20.0f;
constexpr float DRAG_MIN = 5;
constexpr float DEFAULT_DPI = 72.0f;
constexpr float ZOOM_MIN = 0.1f;
constexpr float ZOOM_MAX = 40.0f;
constexpr float ZOOM_STEP = 0.1f;
constexpr float ZOOM_DEFAULT = 1.0f;
constexpr float VERTEX_SIZE = 0.1f;
constexpr Color VERTEX_COLOR = { 0.95f, 0.95f, 0.95f, 1.0f};
constexpr Color VIEW_COLOR = {0.05f, 0.05f, 0.05f, 1.0f};
constexpr float BOX_SELECT_EDGE_WIDTH = 0.005f;
constexpr Color BOX_SELECT_COLOR = Color {0.2f, 0.6f, 1.0f, 0.025f};
constexpr Color BOX_SELECT_OUTLINE_COLOR = Color {0.2f, 0.6f, 1.0f, 0.2f};
constexpr float FRAME_VIEW_PERCENTAGE = 1.0f / 0.75f;

extern void SetPosition(EditorMesh* em, int index, const Vec2& position);
extern int SplitEdge(EditorMesh* em, int edge_index, float edge_pos);
extern void DeleteVertex(EditorMesh* em, int vertex_index);
extern void RotateEdge(EditorMesh* em, int edge_index);
extern void SetTriangleColor(EditorMesh* em, int index, const Vec2Int& color);
extern Vec2 SnapToGrid(const Vec2& position, bool secondary);

static ViewState GetState() { return g_view.state_stack[g_view.state_stack_count-1]; }
static EditorAsset& GetEditingAsset() { return *g_view.assets[g_view.edit_asset_index]; }

View g_view = {};

static void HandleFrame();
static void HandleUIZoomIn();
static void HandleUIZoomOut();

static Shortcut g_common_shortcuts[] = {
    { KEY_F, false, false, false, HandleFrame },
    { KEY_EQUALS, false, true, false, HandleUIZoomIn },
    { KEY_MINUS, false, true, false, HandleUIZoomOut },
    { INPUT_CODE_NONE }
};

static void UpdateCamera()
{
    float DPI = g_view.dpi * g_view.ui_scale * g_view.zoom;
    Vec2Int screen_size = GetScreenSize();
    f32 world_width = screen_size.x / DPI;
    f32 world_height = screen_size.y / ((f32)screen_size.y * DPI / (f32)screen_size.y);
    f32 half_width = world_width * 0.5f;
    f32 half_height = world_height * 0.5f;
    SetExtents(g_view.camera, -half_width, half_width, half_height, -half_height, false);

    g_view.zoom_ref_scale = 1.0f / g_view.zoom;
    g_view.select_size = Abs((ScreenToWorld(g_view.camera, Vec2{0, SELECT_SIZE}) - ScreenToWorld(g_view.camera, VEC2_ZERO)).y);
}

static void FrameView()
{
    Bounds2 bounds = {};
    bool first = true;

    if (g_view.edit_asset_index != -1)
    {
        EditorAsset& ea = GetEditingAsset();
        bounds = GetSelectedBounds(ea) + ea.position;
        first = GetSize(bounds) == VEC2_ZERO;
    }

    if (first)
    {
        for (u32 i=0; i<g_view.asset_count; i++)
        {
            const EditorAsset& ea = *g_view.assets[i];
            if (!ea.selected)
                continue;

            if (first)
                bounds = GetBounds(ea) + ea.position;
            else
                bounds = Union(bounds, GetBounds(ea) + ea.position);

            first = false;
        }
    }

    Vec2 center = GetCenter(bounds);
    Vec2 size = GetSize(bounds);

    f32 max_dimension = Max(size.x, size.y);
    if (max_dimension < ZOOM_MIN)
        max_dimension = ZOOM_MIN;

    Vec2Int screen_size = GetScreenSize();
    f32 target_world_height = max_dimension * FRAME_VIEW_PERCENTAGE;
    g_view.zoom = Clamp((f32)screen_size.y / (g_view.dpi * g_view.ui_scale * target_world_height), ZOOM_MIN, ZOOM_MAX);

    SetPosition(g_view.camera, center);
    UpdateCamera();
}

void BeginBoxSelect(void (*callback)(const Bounds2& bounds))
{
    g_view.box_select_callback = callback;
    PushState(ASSET_EDITOR_STATE_BOX_SELECT);
}

static void HandleBoxSelect()
{
    if (g_view.box_select_callback)
    {
        auto box_select_callback = g_view.box_select_callback;
        g_view.box_select_callback = nullptr;
        box_select_callback(g_view.box_selection);
        return;
    }

    ClearAssetSelection();

    for (u32 i=0; i<g_view.asset_count; i++)
    {
        EditorAsset& ea = *g_view.assets[i];
        if (HitTestAsset(ea, g_view.box_selection))
            AddAssetSelection(i);
    }
}

static void UpdateBoxSelect()
{
    if (!g_view.drag)
    {
        PopState();
        HandleBoxSelect();
        return;
    }

    g_view.box_selection.min = Min(g_view.drag_world_position, g_view.mouse_world_position);
    g_view.box_selection.max = Max(g_view.drag_world_position, g_view.mouse_world_position);
}

static void UpdatePanState()
{
    if (WasButtonPressed(g_view.input, MOUSE_RIGHT))
    {
        g_view.pan_position = g_view.mouse_position;
        g_view.pan_position_camera = GetPosition(g_view.camera);
    }

    if (IsButtonDown(g_view.input, MOUSE_RIGHT))
    {
        Vec2 delta = g_view.mouse_position - g_view.pan_position;
        Vec2 world_delta = ScreenToWorld(g_view.camera, delta) - ScreenToWorld(g_view.camera, VEC2_ZERO);
        SetPosition(g_view.camera, g_view.pan_position_camera - world_delta);
    }
}

static void UpdateZoom()
{
    float zoom_axis = GetAxis(g_view.input, MOUSE_SCROLL_Y);
    if (zoom_axis > -0.5f && zoom_axis < 0.5f)
        return;

    Vec2 mouse_screen = GetMousePosition();
    Vec2 world_under_cursor = ScreenToWorld(g_view.camera, mouse_screen);

    f32 zoom_factor = 1.0f + zoom_axis * ZOOM_STEP;
    g_view.zoom *= zoom_factor;
    g_view.zoom = Clamp(g_view.zoom, ZOOM_MIN, ZOOM_MAX);

    UpdateCamera();

    Vec2 new_screen_pos = WorldToScreen(g_view.camera, world_under_cursor);
    Vec2 screen_offset = mouse_screen - new_screen_pos;
    Vec2 world_offset = ScreenToWorld(g_view.camera, screen_offset) - ScreenToWorld(g_view.camera, VEC2_ZERO);
    Bounds2 bounds = GetBounds(g_view.camera);
    Vec2 current_center = Vec2{(bounds.min.x + bounds.max.x) * 0.5f, (bounds.min.y + bounds.max.y) * 0.5f};
    SetPosition(g_view.camera, current_center + world_offset);
}


static void UpdateMoveState()
{
    // Move all selected assets
    Vec2 drag = g_view.mouse_world_position - g_view.move_world_position;
    for (u32 i=0; i<g_view.asset_count; i++)
    {
        EditorAsset& ea = *g_view.assets[i];
        if (!ea.selected)
            continue;

        MoveTo(ea, ea.saved_position + drag);
    }

    // Cancel move?
    if (WasButtonPressed(g_view.input, KEY_ESCAPE))
    {
        for (u32 i=0; i<g_view.asset_count; i++)
        {
            EditorAsset& ea = *g_view.assets[i];
            ea.position = ea.saved_position;
        }

        PopState();
        CancelUndo();
        return;
    }

    // Finish move?
    if (WasButtonPressed(g_view.input, MOUSE_LEFT) || WasButtonPressed(g_view.input, KEY_G))
    {
        PopState();
        return;
    }
}

static void UpdateDefaultState()
{
    // Selection
    if (WasButtonPressed(g_view.input, MOUSE_LEFT))
    {
        g_view.clear_selection_on_release = true;

        int asset_index = HitTestAssets(g_view.mouse_world_position);
        if (asset_index != -1)
        {
            g_view.clear_selection_on_release = false;
            SetAssetSelection(asset_index);
            return;
        }
    }

    if (g_view.drag)
    {
        PushState(ASSET_EDITOR_STATE_BOX_SELECT);
        return;
    }

    if (WasButtonReleased(g_view.input, MOUSE_LEFT) && g_view.clear_selection_on_release)
    {
        ClearAssetSelection();
        return;
    }

    // Enter edit mode
    if (WasButtonPressed(g_view.input, KEY_TAB) &&
        !IsAltDown(g_view.input) &&
        g_view.selected_asset_count == 1)
    {
        g_view.edit_asset_index = GetFirstSelectedAsset();
        assert(g_view.edit_asset_index != -1);
        g_view.assets[g_view.edit_asset_index]->editing = true;

        EditorAsset& ea = *g_view.assets[g_view.edit_asset_index];
        switch (ea.type)
        {
        case EDITOR_ASSET_TYPE_MESH:
            PushState(ASSET_EDITOR_STATE_EDIT);
            InitMeshEditor(ea);
            break;

        case EDITOR_ASSET_TYPE_SKELETON:
            PushState(ASSET_EDITOR_STATE_EDIT);
            InitSkeletonEditor(ea);
            break;

        case EDITOR_ASSET_TYPE_ANIMATION:
            PushState(ASSET_EDITOR_STATE_EDIT);
            InitAnimationEditor(ea);
            break;

        default:
            break;
        }
    }

    // Start an object move
    if (WasButtonPressed(g_view.input, KEY_G) && g_view.selected_asset_count > 0)
    {
        PushState(ASSET_EDITOR_STATE_MOVE);
        return;
    }
}

void PushState(ViewState state)
{
    assert(state != ASSET_EDITOR_STATE_DEFAULT);
    assert(g_view.state_stack_count < STATE_STACK_SIZE);
    g_view.state_stack[g_view.state_stack_count++] = state;

    switch (state)
    {
    case ASSET_EDITOR_STATE_BOX_SELECT:
        UpdateBoxSelect();
        break;

    case ASSET_EDITOR_STATE_MOVE:
        g_view.move_world_position = g_view.mouse_world_position;
        BeginUndoGroup();
        for (u32 i=0; i<g_view.asset_count; i++)
        {
            EditorAsset& ea = *g_view.assets[i];
            RecordUndo(ea);
            ea.saved_position = ea.position;
        }
        EndUndoGroup();
        break;

    default:
        break;
    }
}

void PopState()
{
    assert(g_view.state_stack_count > 1);
    ViewState state = GetState();
    g_view.state_stack_count--;

    switch (state)
    {
    case ASSET_EDITOR_STATE_EDIT:
        g_view.assets[g_view.edit_asset_index]->editing = false;
        g_view.edit_asset_index = -1;
        break;

    default:
        break;
    }
}

static void UpdateMouse()
{
    g_view.mouse_position = GetMousePosition();
    g_view.mouse_world_position = ScreenToWorld(g_view.camera, g_view.mouse_position);

    if (WasButtonPressed(g_view.input, MOUSE_LEFT))
    {
        g_view.drag = false;
        g_view.drag_world_delta = VEC2_ZERO;
        g_view.drag_delta = VEC2_ZERO;
        g_view.drag_position = g_view.mouse_position;
        g_view.drag_world_position = g_view.mouse_world_position;
    }

    if (IsButtonDown(g_view.input, MOUSE_LEFT))
    {
        g_view.drag_delta = g_view.mouse_position - g_view.drag_position;
        g_view.drag_world_delta = g_view.mouse_world_position - g_view.drag_world_position;
        g_view.drag |= Length(g_view.drag_delta) >= DRAG_MIN;
    }
    else
        g_view.drag = false;
}

static void UpdateCommon()
{
    CheckShortcuts(g_common_shortcuts);

    UpdatePanState();

    if (IsButtonDown(g_view.input, MOUSE_MIDDLE))
    {
        Vec2 dir = Normalize(GetScreenCenter() - g_view.mouse_position);
        g_view.light_dir = Vec3{-dir.x, dir.y, 1.0f};
    }

    if (WasButtonPressed(g_view.input, KEY_Z) && IsButtonDown(g_view.input, KEY_LEFT_CTRL))
    {
        Undo();
        return;
    }

    if (WasButtonPressed(g_view.input, KEY_Y) && IsButtonDown(g_view.input, KEY_LEFT_CTRL))
    {
        Redo();
        return;
    }
}

static void UpdateViewInternal()
{
    UpdateCamera();
    UpdateMouse();
    UpdateCommon();

    switch (GetState())
    {
    case ASSET_EDITOR_STATE_EDIT:
    {
        if (WasButtonPressed(g_view.input, KEY_TAB) && !IsAltDown(g_view.input))
        {
            SetCursor(SYSTEM_CURSOR_DEFAULT);
            PopState();
            return;
        }

        EditorAsset& ea = *g_view.assets[g_view.edit_asset_index];
        switch (ea.type)
        {
        case EDITOR_ASSET_TYPE_MESH:
            UpdateMeshEditor(ea);
            break;

        case EDITOR_ASSET_TYPE_SKELETON:
            UpdateSkeletonEditor();
            break;

        case EDITOR_ASSET_TYPE_ANIMATION:
            UpdateAnimationEditor();
            break;

        default:
            break;
        }
        break;
    }

    case ASSET_EDITOR_STATE_MOVE:
    {
        UpdateMoveState();
        return;
    }

    case ASSET_EDITOR_STATE_BOX_SELECT:
        UpdateBoxSelect();
        break;

    default:
        UpdateDefaultState();
        break;
    }

    // Save
    if (WasButtonPressed(g_view.input, KEY_S) && IsButtonDown(g_view.input, KEY_LEFT_CTRL))
        SaveEditorAssets();

    UpdateZoom();
    UpdateNotifications();
}

static void DrawBoxSelect()
{
    if (GetState() != ASSET_EDITOR_STATE_BOX_SELECT)
        return;

    Vec2 center = GetCenter(g_view.box_selection);
    Vec2 size = GetSize(g_view.box_selection);

    // center
    BindColor(BOX_SELECT_COLOR);
    BindMaterial(g_view.vertex_material);
    BindTransform(TRS(center, 0, size * 0.5f));
    DrawMesh(g_view.edge_mesh);

    // outline
    float edge_width = g_view.zoom_ref_scale * BOX_SELECT_EDGE_WIDTH;
    BindColor(Color{0.2f, 0.6f, 1.0f, 0.8f});
    BindTransform(TRS(Vec2{center.x, g_view.box_selection.max.y}, 0, Vec2{size.x * 0.5f + edge_width, edge_width}));
    DrawMesh(g_view.edge_mesh);
    BindTransform(TRS(Vec2{center.x, g_view.box_selection.min.y}, 0, Vec2{size.x * 0.5f + edge_width, edge_width}));
    DrawMesh(g_view.edge_mesh);
    BindTransform(TRS(Vec2{g_view.box_selection.min.x, center.y}, 0, Vec2{edge_width, size.y * 0.5f + edge_width}));
    DrawMesh(g_view.edge_mesh);
    BindTransform(TRS(Vec2{g_view.box_selection.max.x, center.y}, 0, Vec2{edge_width, size.y * 0.5f + edge_width}));
    DrawMesh(g_view.edge_mesh);
}

void RenderView()
{
    BindCamera(g_view.camera);
    BindLight(g_view.light_dir, COLOR_WHITE, COLOR_BLACK);

    // Grid
    DrawGrid(g_view.camera);

    // Draw assets
    BindColor(COLOR_WHITE);
    BindMaterial(g_view.material);
    for (u32 i=0; i<g_view.asset_count; i++)
        DrawAsset(*g_view.assets[i]);

    // Draw edges
    if (g_view.edit_asset_index != -1)
    {
        EditorAsset& ea = *g_view.assets[g_view.edit_asset_index];
        switch (ea.type)
        {
        case EDITOR_ASSET_TYPE_MESH:
            DrawMeshEditor(ea);
            break;

        case EDITOR_ASSET_TYPE_SKELETON:
            DrawSkeletonEditor();
            break;

        case EDITOR_ASSET_TYPE_ANIMATION:
            DrawAnimationEditor();
            break;

        default:
            break;
        }
    }
    else
    {
        for (u32 i=0; i<g_view.asset_count; i++)
        {
            const EditorAsset& ea = *g_view.assets[i];
            if (!ea.selected)
                continue;

            DrawEdges(ea, 1, COLOR_SELECTED);
        }
    }

    // Draw origins and bounds
    for (u32 i=0; i<g_view.asset_count; i++)
        if (g_view.assets[i]->selected)
        {
            EditorAsset& ea = *g_view.assets[i];
            DrawOrigin(ea);
            DrawBounds(ea, 0.05f);
        }

    DrawBoxSelect();
}

void FocusAsset(int asset_index)
{
    if (g_view.edit_asset_index != -1)
        return;

    ClearAssetSelection();
    SetAssetSelection(asset_index);
    FrameView();
}

void UpdateCommandPalette()
{
    const TextInput& input = GetTextInput();

    if (!g_view.command_palette)
    {
        if (input.value[0] == ':')
        {
            g_view.command_palette = true;
            TextInput clipped = {};
            Copy(clipped.value, TEXT_INPUT_MAX_LENGTH, input.value + 1);
            clipped.length = input.length - 1;
            clipped.cursor = input.cursor - 1;
            SetTextInput(clipped);
            PushInputSet(g_view.command_input);
        }
        else
        {
            ClearTextInput();
            return;
        }
    }

    if (WasButtonPressed(g_view.command_input, KEY_ESCAPE))
    {
        g_view.command_palette = false;
        PopInputSet();
        return;
    }

    if (WasButtonPressed(g_view.command_input, KEY_ENTER))
    {
        PopInputSet();
        g_view.command_palette = false;
        HandleCommand(GetTextInput().value);
        return;
    }

    BeginCanvas();
    SetStyleSheet(g_assets.ui.command_palette);
    BeginElement(g_names.command_palette);
        BeginElement(g_names.command_input);
            Label(":", g_names.command_colon);
            Label(input.value, g_names.command_text);
            BeginElement(g_names.command_text_cursor);
            EndElement();
        EndElement();
    EndElement();
    EndCanvas();
}

void UpdateView()
{
    BeginUI(UI_REF_WIDTH, UI_REF_HEIGHT);
    UpdateViewInternal();
    UpdateCommandPalette();
    EndUI();

    BeginRenderFrame(VIEW_COLOR);
    RenderView();
    DrawVfx();
    DrawUI();
    EndRenderFrame();
}


static void HandleFrame()
{
    if (g_view.selected_asset_count <= 0)
        return;

    FrameView();
}

static void HandleUIZoomIn()
{
    g_view.ui_scale = Min(g_view.ui_scale + 0.1f, 3.0f);
}

static void HandleUIZoomOut()
{
    g_view.ui_scale = Max(g_view.ui_scale - 0.1f, 0.3f);
}

void InitView()
{
    InitUndo();
    InitWindow();

    g_view.camera = CreateCamera(ALLOCATOR_DEFAULT);
    g_view.material = CreateMaterial(ALLOCATOR_DEFAULT, g_assets.shaders.lit);
    g_view.vertex_material = CreateMaterial(ALLOCATOR_DEFAULT, g_assets.shaders.ui);
    g_view.zoom = ZOOM_DEFAULT;
    g_view.ui_scale = 1.0f;
    g_view.dpi = 72.0f;
    g_view.selected_vertex = -1;
    g_view.edit_asset_index = -1;
    UpdateCamera();
    SetTexture(g_view.material, g_assets.textures.palette, 0);

    g_view.input = CreateInputSet(ALLOCATOR_DEFAULT);
    EnableShortcuts(g_common_shortcuts);
    EnableButton(g_view.input, KEY_LEFT_CTRL);
    EnableButton(g_view.input, KEY_LEFT_SHIFT);
    EnableButton(g_view.input, KEY_LEFT_ALT);
    EnableButton(g_view.input, KEY_RIGHT_CTRL);
    EnableButton(g_view.input, KEY_RIGHT_SHIFT);
    EnableButton(g_view.input, KEY_RIGHT_ALT);

    EnableButton(g_view.input, MOUSE_LEFT);
    EnableButton(g_view.input, MOUSE_RIGHT);
    EnableButton(g_view.input, MOUSE_MIDDLE);
    EnableButton(g_view.input, KEY_X);
    EnableButton(g_view.input, KEY_F);
    EnableButton(g_view.input, KEY_G);
    EnableButton(g_view.input, KEY_R);
    EnableButton(g_view.input, KEY_M);
    EnableButton(g_view.input, KEY_Q);
    EnableButton(g_view.input, KEY_0);
    EnableButton(g_view.input, KEY_1);
    EnableButton(g_view.input, KEY_A);
    EnableButton(g_view.input, KEY_V);
    EnableButton(g_view.input, KEY_ESCAPE);
    EnableButton(g_view.input, KEY_ENTER);
    EnableButton(g_view.input, KEY_SPACE);
    EnableButton(g_view.input, KEY_SEMICOLON);
    EnableButton(g_view.input, KEY_TAB);
    EnableButton(g_view.input, KEY_S);
    EnableButton(g_view.input, KEY_Z);
    EnableButton(g_view.input, KEY_Y);
    EnableButton(g_view.input, KEY_EQUALS);
    EnableButton(g_view.input, KEY_MINUS);
    PushInputSet(g_view.input);

    g_view.command_input = CreateInputSet(ALLOCATOR_DEFAULT);
    EnableButton(g_view.command_input, KEY_ESCAPE);
    EnableButton(g_view.command_input, KEY_ENTER);

    MeshBuilder* builder = CreateMeshBuilder(ALLOCATOR_DEFAULT, 4, 6);
    AddVertex(builder, {   0, -0.5f}, {0,0,1}, VEC2_ZERO);
    AddVertex(builder, { 0.5f, 0.0f}, {0,0,1}, VEC2_ZERO);
    AddVertex(builder, {   0,  0.5f}, {0,0,1}, VEC2_ZERO);
    AddVertex(builder, {-0.5f, 0.0f}, {0,0,1}, VEC2_ZERO);
    AddTriangle(builder, 0, 1, 2);
    AddTriangle(builder, 2, 3, 0);
    g_view.vertex_mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE);

    Clear(builder);
    AddVertex(builder, { -1, -1});
    AddVertex(builder, {  1, -1});
    AddVertex(builder, {  1,  1});
    AddVertex(builder, { -1,  1});
    AddTriangle(builder, 0, 1, 2);
    AddTriangle(builder, 2, 3, 0);
    g_view.edge_mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE);

    Free(builder);

    InitGrid(ALLOCATOR_DEFAULT);
    InitNotifications();
    LoadEditorAssets();
    g_view.state_stack[0] = ASSET_EDITOR_STATE_DEFAULT;
    g_view.state_stack_count = 1;
}

void ShutdownView()
{
    g_view = {};

    ShutdownGrid();
    ShutdownWindow();
    ShutdownUndo();
}