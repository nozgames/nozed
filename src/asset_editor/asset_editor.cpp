//
//  MeshZ - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"
#include "file_helpers.h"

constexpr int MAX_COMMAND_LENGTH = 1024;
constexpr float DRAG_MIN = 1;
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

extern void SetPosition(EditableMesh* em, int index, const Vec2& position);
extern int SplitEdge(EditableMesh* em, int edge_index, float edge_pos);
extern void DeleteVertex(EditableMesh* em, int vertex_index);
extern void RotateEdge(EditableMesh* em, int edge_index);
extern void SetTriangleColor(EditableMesh* em, int index, const Vec2Int& color);
extern Vec2 SnapToGrid(const Vec2& position, bool secondary);

static AssetEditorState GetState() { return g_asset_editor.state_stack[g_asset_editor.state_stack_count-1]; }
static EditableAsset& GetEditingAsset() { return *g_asset_editor.assets[g_asset_editor.edit_asset_index]; }

AssetEditor g_asset_editor = {};

static void UpdateCamera()
{
    float DPI = g_asset_editor.dpi * g_asset_editor.ui_scale * g_asset_editor.zoom;
    Vec2Int screen_size = GetScreenSize();
    f32 world_width = screen_size.x / DPI;
    f32 world_height = screen_size.y / ((f32)screen_size.y * DPI / (f32)screen_size.y);
    f32 half_width = world_width * 0.5f;
    f32 half_height = world_height * 0.5f;
    SetExtents(g_asset_editor.camera, -half_width, half_width, -half_height, half_height, false);

    g_asset_editor.zoom_ref_scale = 1.0f / g_asset_editor.zoom;
}

static void FrameView()
{
    Bounds2 bounds = {};
    bool first = true;

    if (g_asset_editor.edit_asset_index != -1)
    {
        EditableAsset& ea = GetEditingAsset();
        bounds = GetSelectedBounds(ea) + ea.position;
        first = GetSize(bounds) == VEC2_ZERO;
    }

    if (first)
    {
        for (int i=0; i<g_asset_editor.asset_count; i++)
        {
            const EditableAsset& ea = *g_asset_editor.assets[i];
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
    g_asset_editor.zoom = (f32)screen_size.y / (g_asset_editor.dpi * g_asset_editor.ui_scale * target_world_height);
    
    SetPosition(g_asset_editor.camera, center);
    UpdateCamera();
}

static void HandleBoxSelect()
{
    // When in edit mode let the editor handle it
    if (GetState() == ASSET_EDITOR_STATE_EDIT && g_asset_editor.edit_asset_index != -1)
    {
        EditableAsset& ea = *g_asset_editor.assets[g_asset_editor.edit_asset_index];
        switch (ea.type)
        {
        case EDITABLE_ASSET_TYPE_MESH:
        {
            HandleMeshEditorBoxSelect(ea, g_asset_editor.box_selection);
            break;
        }

        default:
            break;
        }

        return;
    }

    ClearAssetSelection();
    for (int i=0; i<g_asset_editor.asset_count; i++)
    {
        EditableAsset& ea = *g_asset_editor.assets[i];
        if (HitTestAsset(ea, g_asset_editor.box_selection))
            AddAssetSelection(i);
    }
}

static void UpdateBoxSelect()
{
    if (!g_asset_editor.drag)
    {
        PopState();
        HandleBoxSelect();
        return;
    }

    g_asset_editor.box_selection.min = Min(g_asset_editor.drag_world_position, g_asset_editor.mouse_world_position);
    g_asset_editor.box_selection.max = Max(g_asset_editor.drag_world_position, g_asset_editor.mouse_world_position);
}

static void UpdatePanState()
{
    if (WasButtonReleased(g_asset_editor.input, KEY_SPACE))
    {
        PopState();
        return;
    }

    if (g_asset_editor.drag)
    {
        Vec2 delta = g_asset_editor.mouse_position - g_asset_editor.drag_position;
        Vec2 world_delta = ScreenToWorld(g_asset_editor.camera, delta) - ScreenToWorld(g_asset_editor.camera, VEC2_ZERO);
        SetPosition(g_asset_editor.camera, g_asset_editor.pan_start - world_delta);
    }
}

static void ZoomView()
{
    float zoom_axis = GetAxis(g_asset_editor.input, MOUSE_SCROLL_Y);
    if (zoom_axis > -0.5f && zoom_axis < 0.5f)
        return;

    Vec2 mouse_screen = GetMousePosition();
    Vec2 world_under_cursor = ScreenToWorld(g_asset_editor.camera, mouse_screen);

    f32 zoom_factor = 1.0f + zoom_axis * ZOOM_STEP;
    g_asset_editor.zoom *= zoom_factor;
    g_asset_editor.zoom = Clamp(g_asset_editor.zoom, ZOOM_MIN, ZOOM_MAX);

    UpdateCamera();

    Vec2 new_screen_pos = WorldToScreen(g_asset_editor.camera, world_under_cursor);
    Vec2 screen_offset = mouse_screen - new_screen_pos;
    Vec2 world_offset = ScreenToWorld(g_asset_editor.camera, screen_offset) - ScreenToWorld(g_asset_editor.camera, VEC2_ZERO);
    Bounds2 bounds = GetBounds(g_asset_editor.camera);
    Vec2 current_center = Vec2{(bounds.min.x + bounds.max.x) * 0.5f, (bounds.min.y + bounds.max.y) * 0.5f};
    SetPosition(g_asset_editor.camera, current_center + world_offset);
}

static void UpdateView()
{
    ZoomView();

    // Frame
    if (WasButtonPressed(g_asset_editor.input, KEY_F) && g_asset_editor.selected_asset_count > 0)
        FrameView();

    // UI Scale controls
    if (IsButtonDown(g_asset_editor.input, KEY_LEFT_CTRL))
    {
        // Handle both = and + (shift + =) for increasing UI scale
        if (WasButtonPressed(g_asset_editor.input, KEY_EQUALS))
        {
            g_asset_editor.ui_scale = Min(g_asset_editor.ui_scale + 0.1f, 3.0f);
        }
        if (WasButtonPressed(g_asset_editor.input, KEY_MINUS))
        {
            g_asset_editor.ui_scale = Max(g_asset_editor.ui_scale - 0.1f, 0.3f);
        }
    }
}

static void UpdateMoveState()
{
    // Move all selected assets
    Vec2 drag = g_asset_editor.mouse_world_position - g_asset_editor.move_world_position;
    for (int i=0; i<g_asset_editor.asset_count; i++)
    {
        EditableAsset& ea = *g_asset_editor.assets[i];
        if (!ea.selected)
            continue;

        MoveTo(ea, ea.saved_position + drag);
    }

    // Cancel move?
    if (WasButtonPressed(g_asset_editor.input, KEY_ESCAPE))
    {
        for (int i=0; i<g_asset_editor.asset_count; i++)
        {
            EditableAsset& ea = *g_asset_editor.assets[i];
            ea.position = ea.saved_position;
        }

        PopState();
        CancelUndo();
        return;
    }

    // Finish move?
    if (WasButtonPressed(g_asset_editor.input, MOUSE_LEFT) || WasButtonPressed(g_asset_editor.input, KEY_G))
    {
        PopState();
        return;
    }
}

static void UpdateDefaultState()
{
    // Selection
    if (WasButtonPressed(g_asset_editor.input, MOUSE_LEFT))
    {
        g_asset_editor.clear_selection_on_release = true;

        int asset_index = HitTestAssets(g_asset_editor.mouse_world_position);
        if (asset_index != -1)
        {
            g_asset_editor.clear_selection_on_release = false;
            SetAssetSelection(asset_index);
            return;
        }
    }

    if (g_asset_editor.drag)
    {
        PushState(ASSET_EDITOR_STATE_BOX_SELECT);
        return;
    }

    if (WasButtonReleased(g_asset_editor.input, MOUSE_LEFT) && g_asset_editor.clear_selection_on_release)
    {
        ClearAssetSelection();
        return;
    }

    if (WasButtonPressed(g_asset_editor.input, KEY_SPACE))
    {
        PushState(ASSET_EDITOR_STATE_PAN);
        return;
    }

    // Enter edit mode
    if (WasButtonPressed(g_asset_editor.input, KEY_TAB) &&
        !IsButtonDown(g_asset_editor.input, KEY_LEFT_ALT) &&
        g_asset_editor.selected_asset_count == 1)
    {
        g_asset_editor.edit_asset_index = GetFirstSelectedAsset();
        assert(g_asset_editor.edit_asset_index != -1);

        EditableAsset& ea = *g_asset_editor.assets[g_asset_editor.edit_asset_index];
        switch (ea.type)
        {
        case EDITABLE_ASSET_TYPE_MESH:
            PushState(ASSET_EDITOR_STATE_EDIT);
            InitMeshEditor(ea);
            break;

        default:
            break;
        }
    }

    // Start an object move
    if (WasButtonPressed(g_asset_editor.input, KEY_G) && g_asset_editor.selected_asset_count > 0)
    {
        PushState(ASSET_EDITOR_STATE_MOVE);
        return;
    }
}

void PushState(AssetEditorState state)
{
    assert(state != ASSET_EDITOR_STATE_DEFAULT);
    assert(g_asset_editor.state_stack_count < STATE_STACK_SIZE);
    g_asset_editor.state_stack[g_asset_editor.state_stack_count++] = state;

    switch (state)
    {
    case ASSET_EDITOR_STATE_BOX_SELECT:
        UpdateBoxSelect();
        break;

    case ASSET_EDITOR_STATE_MOVE:
        g_asset_editor.move_world_position = g_asset_editor.mouse_world_position;
        BeginUndoGroup();
        for (int i=0; i<g_asset_editor.asset_count; i++)
        {
            EditableAsset& ea = *g_asset_editor.assets[i];
            RecordUndo(ea);
            ea.saved_position = ea.position;
        }
        EndUndoGroup();
        break;

    case ASSET_EDITOR_STATE_PAN:
        g_asset_editor.move_world_position = g_asset_editor.mouse_world_position;
        g_asset_editor.pan_start = GetPosition(g_asset_editor.camera);
        break;

    default:
        break;
    }
}

static void PopState()
{
    assert(g_asset_editor.state_stack_count > 1);
    AssetEditorState state = GetState();
    g_asset_editor.state_stack_count--;

    switch (state)
    {
    case ASSET_EDITOR_STATE_EDIT:
        g_asset_editor.edit_asset_index = -1;
        break;

    default:
        break;
    }
}

static void UpdateMouse()
{
    g_asset_editor.mouse_position = GetMousePosition();
    g_asset_editor.mouse_world_position = ScreenToWorld(g_asset_editor.camera, g_asset_editor.mouse_position);

    if (WasButtonPressed(g_asset_editor.input, MOUSE_LEFT))
    {
        g_asset_editor.drag = false;
        g_asset_editor.drag_world_delta = VEC2_ZERO;
        g_asset_editor.drag_delta = VEC2_ZERO;
        g_asset_editor.drag_position = g_asset_editor.mouse_position;
        g_asset_editor.drag_world_position = g_asset_editor.mouse_world_position;
    }

    if (IsButtonDown(g_asset_editor.input, MOUSE_LEFT))
    {
        g_asset_editor.drag_delta = g_asset_editor.mouse_position - g_asset_editor.drag_position;
        g_asset_editor.drag_world_delta = g_asset_editor.mouse_world_position - g_asset_editor.drag_world_position;
        g_asset_editor.drag |= Length(g_asset_editor.drag_delta) >= DRAG_MIN;
    }
    else
        g_asset_editor.drag = false;
}

static void UpdateCommon()
{
    if (WasButtonPressed(g_asset_editor.input, KEY_Z) && IsButtonDown(g_asset_editor.input, KEY_LEFT_CTRL))
    {
        Undo();
        return;
    }

    if (WasButtonPressed(g_asset_editor.input, KEY_Y) && IsButtonDown(g_asset_editor.input, KEY_LEFT_CTRL))
    {
        Redo();
        return;
    }
}

static void UpdateAssetEditorInternal()
{
    UpdateCamera();
    UpdateMouse();
    UpdateCommon();

    switch (GetState())
    {
    case ASSET_EDITOR_STATE_EDIT:
    {
        if (WasButtonPressed(g_asset_editor.input, KEY_TAB))
        {
            PopState();
            return;
        }

        EditableAsset& ea = *g_asset_editor.assets[g_asset_editor.edit_asset_index];
        switch (ea.type)
        {
        case EDITABLE_ASSET_TYPE_MESH:
            UpdateMeshEditor(ea);
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

    case ASSET_EDITOR_STATE_PAN:
        UpdatePanState();
        break;

    default:
        UpdateDefaultState();
        break;
    }

    // Save
    if (WasButtonPressed(g_asset_editor.input, KEY_S) && IsButtonDown(g_asset_editor.input, KEY_LEFT_CTRL))
        SaveEditableAssets();

    UpdateView();
    UpdateNotifications();
}

static void DrawBoxSelect()
{
    if (GetState() != ASSET_EDITOR_STATE_BOX_SELECT)
        return;

    Vec2 center = GetCenter(g_asset_editor.box_selection);
    Vec2 size = GetSize(g_asset_editor.box_selection);

    // center
    BindColor(BOX_SELECT_COLOR);
    BindMaterial(g_asset_editor.vertex_material);
    BindTransform(TRS(center, 0, size * 0.5f));
    DrawMesh(g_asset_editor.edge_mesh);

    // outline
    float edge_width = g_asset_editor.zoom_ref_scale * BOX_SELECT_EDGE_WIDTH;
    BindColor(Color{0.2f, 0.6f, 1.0f, 0.8f});
    BindTransform(TRS(Vec2{center.x, g_asset_editor.box_selection.max.y}, 0, Vec2{size.x * 0.5f + edge_width, edge_width}));
    DrawMesh(g_asset_editor.edge_mesh);
    BindTransform(TRS(Vec2{center.x, g_asset_editor.box_selection.min.y}, 0, Vec2{size.x * 0.5f + edge_width, edge_width}));
    DrawMesh(g_asset_editor.edge_mesh);
    BindTransform(TRS(Vec2{g_asset_editor.box_selection.min.x, center.y}, 0, Vec2{edge_width, size.y * 0.5f + edge_width}));
    DrawMesh(g_asset_editor.edge_mesh);
    BindTransform(TRS(Vec2{g_asset_editor.box_selection.max.x, center.y}, 0, Vec2{edge_width, size.y * 0.5f + edge_width}));
    DrawMesh(g_asset_editor.edge_mesh);
}

void RenderView()
{
    BindCamera(g_asset_editor.camera);

    // Grid
    DrawGrid(g_asset_editor.camera);

    // Draw assets
    BindColor(COLOR_WHITE);
    BindMaterial(g_asset_editor.material);
    for (int i=0; i<g_asset_editor.asset_count; i++)
        DrawAsset(*g_asset_editor.assets[i]);

    // Draw edges
    if (g_asset_editor.edit_asset_index != -1)
    {
        EditableAsset& ea = *g_asset_editor.assets[g_asset_editor.edit_asset_index];
        switch (ea.type)
        {
        case EDITABLE_ASSET_TYPE_MESH:
            DrawMeshEditor(ea);
            break;

        default:
            break;
        }
    }
    else
    {
        for (int i=0; i<g_asset_editor.asset_count; i++)
        {
            const EditableAsset& ea = *g_asset_editor.assets[i];
            if (!ea.selected)
                continue;

            DrawEdges(ea, 1, COLOR_SELECTED);
        }
    }

    DrawBoxSelect();
}

void FocusAsset(int asset_index)
{
    if (g_asset_editor.edit_asset_index != -1)
        return;

    ClearAssetSelection();
    SetAssetSelection(asset_index);
    FrameView();
}

void UpdateCommandPalette()
{
    const TextInput& input = GetTextInput();

    if (!g_asset_editor.command_palette)
    {
        if (input.value[0] == ':')
        {
            g_asset_editor.command_palette = true;
            TextInput clipped = {};
            Copy(clipped.value, TEXT_INPUT_MAX_LENGTH, input.value + 1);
            clipped.length = input.length - 1;
            clipped.cursor = input.cursor - 1;
            SetTextInput(clipped);
            PushInputSet(g_asset_editor.command_input);
        }
        else
        {
            ClearTextInput();
            return;
        }
    }

    if (WasButtonPressed(g_asset_editor.command_input, KEY_ESCAPE))
    {
        g_asset_editor.command_palette = false;
        PopInputSet();
        return;
    }

    if (WasButtonPressed(g_asset_editor.command_input, KEY_ENTER))
    {
        PopInputSet();
        g_asset_editor.command_palette = false;
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

void UpdateAssetEditor()
{
    BeginUI(UI_REF_WIDTH, UI_REF_HEIGHT);
    UpdateAssetEditorInternal();
    UpdateCommandPalette();
    EndUI();

    BeginRenderFrame(VIEW_COLOR);
    RenderView();
    DrawUI();
    EndRenderFrame();
}


void InitAssetEditor()
{
    InitUndo();
    InitWindow();

    g_asset_editor.camera = CreateCamera(ALLOCATOR_DEFAULT);
    g_asset_editor.material = CreateMaterial(ALLOCATOR_DEFAULT, g_assets.shaders._default);
    g_asset_editor.vertex_material = CreateMaterial(ALLOCATOR_DEFAULT, g_assets.shaders.ui);
    g_asset_editor.zoom = ZOOM_DEFAULT;
    g_asset_editor.ui_scale = 1.0f;
    g_asset_editor.dpi = 72.0f;
    g_asset_editor.selected_vertex = -1;
    g_asset_editor.edit_asset_index = -1;
    UpdateCamera();
    SetTexture(g_asset_editor.material, g_assets.textures.palette, 0);

    g_asset_editor.input = CreateInputSet(ALLOCATOR_DEFAULT);
    EnableButton(g_asset_editor.input, MOUSE_LEFT);
    EnableButton(g_asset_editor.input, MOUSE_RIGHT);
    EnableButton(g_asset_editor.input, MOUSE_MIDDLE);
    EnableButton(g_asset_editor.input, KEY_X);
    EnableButton(g_asset_editor.input, KEY_F);
    EnableButton(g_asset_editor.input, KEY_G);
    EnableButton(g_asset_editor.input, KEY_R);
    EnableButton(g_asset_editor.input, KEY_M);
    EnableButton(g_asset_editor.input, KEY_Q);
    EnableButton(g_asset_editor.input, KEY_0);
    EnableButton(g_asset_editor.input, KEY_1);
    EnableButton(g_asset_editor.input, KEY_A);
    EnableButton(g_asset_editor.input, KEY_V);
    EnableButton(g_asset_editor.input, KEY_ESCAPE);
    EnableButton(g_asset_editor.input, KEY_ENTER);
    EnableButton(g_asset_editor.input, KEY_SPACE);
    EnableButton(g_asset_editor.input, KEY_SEMICOLON);
    EnableButton(g_asset_editor.input, KEY_LEFT_CTRL);
    EnableButton(g_asset_editor.input, KEY_LEFT_SHIFT);
    EnableButton(g_asset_editor.input, KEY_RIGHT_SHIFT);
    EnableButton(g_asset_editor.input, KEY_TAB);
    EnableButton(g_asset_editor.input, KEY_LEFT_ALT);
    EnableButton(g_asset_editor.input, KEY_S);
    EnableButton(g_asset_editor.input, KEY_Z);
    EnableButton(g_asset_editor.input, KEY_Y);
    EnableButton(g_asset_editor.input, KEY_EQUALS);
    EnableButton(g_asset_editor.input, KEY_MINUS);
    PushInputSet(g_asset_editor.input);

    g_asset_editor.command_input = CreateInputSet(ALLOCATOR_DEFAULT);
    EnableButton(g_asset_editor.command_input, KEY_ESCAPE);
    EnableButton(g_asset_editor.command_input, KEY_ENTER);

    MeshBuilder* builder = CreateMeshBuilder(ALLOCATOR_DEFAULT, 4, 6);
    AddVertex(builder, {   0, -0.5f}, {0,0,1}, VEC2_ZERO, 0);
    AddVertex(builder, { 0.5f, 0.0f}, {0,0,1}, VEC2_ZERO, 0);
    AddVertex(builder, {   0,  0.5f}, {0,0,1}, VEC2_ZERO, 0);
    AddVertex(builder, {-0.5f, 0.0f}, {0,0,1}, VEC2_ZERO, 0);
    AddTriangle(builder, 0, 1, 2);
    AddTriangle(builder, 2, 3, 0);
    g_asset_editor.vertex_mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE);

    Clear(builder);
    AddVertex(builder, { -1, -1});
    AddVertex(builder, {  1, -1});
    AddVertex(builder, {  1,  1});
    AddVertex(builder, { -1,  1});
    AddTriangle(builder, 0, 1, 2);
    AddTriangle(builder, 2, 3, 0);
    g_asset_editor.edge_mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE);

    Free(builder);

    InitGrid(ALLOCATOR_DEFAULT);
    InitNotifications();

    g_asset_editor.asset_count = LoadEditableAssets(g_asset_editor.assets);
    g_asset_editor.state_stack[0] = ASSET_EDITOR_STATE_DEFAULT;
    g_asset_editor.state_stack_count = 1;
}

void ShutdownAssetEditor()
{
    g_asset_editor = {};

    ShutdownGrid();
    ShutdownWindow();
    ShutdownUndo();
}