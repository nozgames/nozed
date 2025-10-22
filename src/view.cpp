//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "editor.h"
#include "editor_assets.h"

constexpr int MAX_COMMAND_LENGTH = 1024;
constexpr float SELECT_SIZE = 60.0f;
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

View g_view = {};

inline ViewState GetState() { return g_view.state_stack[g_view.state_stack_count-1]; }

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

static Bounds2 GetViewBounds(EditorAsset* ea) {
    if (g_view.edit_asset_index == GetIndex(ea) && g_view.vtable.bounds)
        return g_view.vtable.bounds();

    return GetBounds(ea);
}

static void FrameSelected() {
    Bounds2 bounds = {};
    bool first = true;

    if (first)
    {
        for (u32 i=0; i<MAX_ASSETS; i++)
        {
            EditorAsset* ea = GetEditorAsset(i);
            if (!ea || !ea->selected)
                continue;

            if (first)
                bounds = GetViewBounds(ea) + ea->position;
            else
                bounds = Union(bounds, GetViewBounds(ea) + ea->position);

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

void BeginBoxSelect(void (*callback)(const Bounds2& bounds)) {
    g_view.box_select_callback = callback;
    PushState(VIEW_STATE_BOX_SELECT);
}

static void HandleBoxSelect() {
    if (g_view.box_select_callback)
    {
        auto box_select_callback = g_view.box_select_callback;
        g_view.box_select_callback = nullptr;
        box_select_callback(g_view.box_selection);
        return;
    }

    ClearAssetSelection();

    for (u32 i=0; i<MAX_ASSETS; i++)
    {
        EditorAsset* ea = GetEditorAsset(i);
        if (!ea)
            continue;

        if (OverlapBounds(ea, g_view.box_selection))
            AddAssetSelection(i);
    }
}

static void UpdateBoxSelect() {
    if (!g_view.drag)
    {
        PopState();
        HandleBoxSelect();
        return;
    }

    g_view.box_selection.min = Min(g_view.drag_world_position, g_view.mouse_world_position);
    g_view.box_selection.max = Max(g_view.drag_world_position, g_view.mouse_world_position);
}

static void UpdatePanState() {
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

    // Capture the world position under the cursor before zoom
    Vec2 mouse_screen = GetMousePosition();
    Vec2 world_under_cursor = ScreenToWorld(g_view.camera, mouse_screen);

    // Apply zoom
    f32 zoom_factor = 1.0f + zoom_axis * ZOOM_STEP;
    g_view.zoom *= zoom_factor;
    g_view.zoom = Clamp(g_view.zoom, ZOOM_MIN, ZOOM_MAX);

    // Update camera with new zoom
    UpdateCamera();

    // Calculate where the world point is now in screen space after zoom
    Vec2 world_under_cursor_after = ScreenToWorld(g_view.camera, mouse_screen);

    // Adjust camera position to keep the world point under cursor
    Vec2 current_position = GetPosition(g_view.camera);
    Vec2 world_offset = world_under_cursor - world_under_cursor_after;
    SetPosition(g_view.camera, current_position + world_offset);
}

static void UpdateMoveState()
{
    Vec2 drag = g_view.mouse_world_position - g_view.move_world_position;

    for (u32 i=0; i<MAX_ASSETS; i++)
    {
        EditorAsset* ea = GetEditorAsset(i);
        if (!ea || !ea->selected)
            continue;

        MoveTo(ea, IsCtrlDown(g_view.input) ? SnapToGrid(ea->saved_position + drag, false) : ea->saved_position + drag);
    }

    // Cancel move?
    if (WasButtonPressed(g_view.input, KEY_ESCAPE))
    {
        for (u32 i=0; i<MAX_ASSETS; i++)
        {
            EditorAsset* ea = GetEditorAsset(i);
            if (ea && ea->selected)
                ea->position = ea->saved_position;
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

static void UpdateDefaultState() {
    if (WasButtonPressed(g_view.input, MOUSE_LEFT)) {
        int asset_index = HitTestAssets(g_view.mouse_world_position);
        if (asset_index != -1)
        {
            g_view.clear_selection_on_release = false;
            if (IsShiftDown(g_view.input))
                ToggleAssetSelection(asset_index);
            else
                SetAssetSelection(asset_index);
            return;
        }

        g_view.clear_selection_on_release = !IsShiftDown(g_view.input);
    }

    if (g_view.drag) {
        PushState(VIEW_STATE_BOX_SELECT);
        return;
    }

    if (WasButtonReleased(g_view.input, MOUSE_LEFT) && g_view.clear_selection_on_release) {
        ClearAssetSelection();
        return;
    }

    // Enter edit mode
    if (WasButtonPressed(g_view.input, KEY_TAB) &&
        !IsAltDown(g_view.input) &&
        g_view.selected_asset_count == 1) {
        g_view.edit_asset_index = GetFirstSelectedAsset();
        assert(g_view.edit_asset_index != -1);

        EditorAsset* ea = GetEditingAsset();
        assert(ea);
        ea->editing = true;

        if (ea->vtable.view_init)
        {
            PushState(VIEW_STATE_EDIT);
            ea->vtable.view_init();
        }
    }

    // Start an object move
    if (WasButtonPressed(g_view.input, KEY_G) && g_view.selected_asset_count > 0) {
        PushState(VIEW_STATE_MOVE);
        return;
    }
}

void PushState(ViewState state)
{
    assert(state != VIEW_STATE_DEFAULT);
    assert(g_view.state_stack_count < STATE_STACK_SIZE);
    g_view.state_stack[g_view.state_stack_count++] = state;

    switch (state)
    {
    case VIEW_STATE_BOX_SELECT:
        UpdateBoxSelect();
        break;

    case VIEW_STATE_MOVE:
        g_view.move_world_position = g_view.mouse_world_position;
        BeginUndoGroup();
        for (u32 i=0; i<MAX_ASSETS; i++)
        {
            EditorAsset* ea = GetEditorAsset(i);
            if (!ea || !ea->selected)
                continue;
            RecordUndo(ea);
            ea->saved_position = ea->position;
        }
        EndUndoGroup();
        BeginTextInput();
        break;

    case VIEW_STATE_COMMAND:
        BeginTextInput();
        PushInputSet(g_view.input_command);
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
    case VIEW_STATE_EDIT:
        GetEditingAsset()->editing = false;
        g_view.edit_asset_index = -1;
        g_view.vtable = {};
        break;

    case VIEW_STATE_MOVE:
        EndTextInput();
        break;

    case VIEW_STATE_COMMAND:
        PopInputSet();
        EndTextInput();
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
    CheckShortcuts(g_view.shortcuts);

    UpdatePanState();

    if (IsButtonDown(g_view.input, MOUSE_MIDDLE))
    {
        Vec2 dir = Normalize(GetScreenCenter() - g_view.mouse_position);
        g_view.light_dir = Vec2{-dir.x, dir.y};
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

static void UpdateViewInternal() {
    UpdateCamera();
    UpdateMouse();
    UpdateCommon();

    switch (GetState())
    {
    case VIEW_STATE_EDIT:
        if (WasButtonPressed(g_view.input, KEY_TAB) && !IsAltDown(g_view.input)) {
            if (g_view.vtable.shutdown)
                g_view.vtable.shutdown();

            SetCursor(SYSTEM_CURSOR_DEFAULT);
            PopState();
            return;
        }

        if (g_view.vtable.update)
            g_view.vtable.update();

        break;

    case VIEW_STATE_MOVE:
        UpdateMoveState();
        return;

    case VIEW_STATE_BOX_SELECT:
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

static void DrawBoxSelect() {
    if (GetState() != VIEW_STATE_BOX_SELECT)
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

void DrawView() {
    BindCamera(g_view.camera);
    BindLight(Normalize(Vec3{g_view.light_dir.x, g_view.light_dir.y, 0.0f}), COLOR_WHITE, COLOR_BLACK);
    DrawGrid(g_view.camera);

    Bounds2 camera_bounds = GetBounds(g_view.camera);
    for (u32 i=0; i<MAX_ASSETS; i++) {
        EditorAsset* ea = GetEditorAsset(i);
        if (!ea)
            continue;

        ea->clipped = !Intersects(camera_bounds, GetBounds(ea) + ea->position);
    }

    bool show_names = g_view.show_names || IsAltDown(g_view.input);
    if (show_names) {
        for (u32 i=0; i<MAX_ASSETS; i++) {
            EditorAsset* ea = GetEditorAsset(i);
            if (!ea)
                continue;

            DrawBounds(ea);
        }
    }

    BindColor(COLOR_WHITE);
    BindMaterial(g_view.shaded_material);
    for (u32 i=0, c=GetEditorAssetCount(); i<c; i++) {
        EditorAsset* ea = GetSortedEditorAsset(i);
        if (!ea || ea->clipped)
            continue;

        if (g_view.edit_asset_index != (int)i || g_view.vtable.draw == nullptr)
            DrawAsset(ea);
    }

    if (g_view.edit_asset_index != -1)
        if (g_view.vtable.draw)
            g_view.vtable.draw();

    for (u32 i=0; i<MAX_ASSETS; i++) {
        EditorAsset* ea = GetEditorAsset(i);
        if (!ea || ea->clipped)
            continue;

        DrawOrigin(ea);
    }

    for (u32 i=0; i<MAX_ASSETS; i++) {
        EditorAsset* ea = GetEditorAsset(i);
        if (!ea || !ea->selected || ea->editing)
            continue;

        DrawBounds(ea, 0, COLOR_VERTEX_SELECTED);
    }

    if (IsButtonDown(g_view.input, MOUSE_MIDDLE)) {
        Bounds2 bounds = GetBounds(g_view.camera);
        DrawDashedLine(g_view.mouse_world_position, GetCenter(bounds));
        BindColor(COLOR_VERTEX_SELECTED);
        DrawVertex(g_view.mouse_world_position);
        DrawVertex(GetCenter(bounds));
    }

    DrawBoxSelect();
}

void FocusAsset(EditorAsset* ea)
{
    assert(ea);

    if (g_view.edit_asset_index != -1)
        return;

    ClearAssetSelection();
    SetAssetSelection(GetIndex(ea));
    FrameSelected();
}

static void UpdateCommandModifier(const TextInput& input) {
    (void) input;
#if 0
    if (GetState() != VIEW_STATE_MODIFIER)
        PushState(VIEW_STATE_MODIFIER);

    if (input.length == 0)
        return;

    Canvas([] {
        Align({.alignment={.y=1}, .margin=EdgeInsetsBottom(40)}, [] {
            Container({.width=300, .height=50, .padding=EdgeInsetsTopLeft(10, 10), .color=Color24ToColor(0x343c4a)}, [] {
                Row([]{
                    const TextInput& i = GetTextInput();
                    Label(i.value, {.font = FONT_SEGUISB, .font_size = 30, .color = Color24ToColor(0xc5c5cb) });
                    Container({.width=4, .height=30, .color=COLOR_WHITE});
                });
            });
        });
    });
#endif
}

static void UpdateCommandPalette() {
    const TextInput& input = GetTextInput();

    if (g_view.vtable.allow_text_input && g_view.vtable.allow_text_input()) {
        UpdateCommandModifier(input);
        return;
    }

    if (GetState() != VIEW_STATE_COMMAND)
        return;

    Canvas([] {
        Align({.alignment={.y=1}, .margin=EdgeInsetsBottom(40)}, [] {
            Container({.width=600, .height=50, .padding=EdgeInsetsTopLeft(10, 10), .color=COLOR_UI_BACKGROUND}, [] {
                Row([]{
                    const TextInput& i = GetTextInput();
                    Label(":", {.font = FONT_SEGUISB, .font_size = 30, .color = Color24ToColor(0x777776), .align=ALIGNMENT_CENTER_LEFT});
                    SizedBox({.width = 5.0f});
                    Label(i.value, {.font = FONT_SEGUISB, .font_size = 30, .color = COLOR_UI_TEXT });
                    if (g_view.command_preview) {
                        Container({.color = COLOR_UI_TEXT}, [] {
                            Label(g_view.command_preview->value, {.font = FONT_SEGUISB, .font_size = 30, .color = COLOR_UI_BACKGROUND });
                        });
                    }
                    else
                        Container({.width=4, .height=30, .color=COLOR_WHITE});
                });
            });
        });
    });
}

static void UpdateAssetNames() {
    if (GetState() != VIEW_STATE_DEFAULT &&
        GetState() != VIEW_STATE_COMMAND &&
        GetState() != VIEW_STATE_BOX_SELECT)
        return;

    if (!IsAltDown(g_view.input) && !g_view.show_names)
        return;

    for (u32 i=0; i<MAX_ASSETS; i++) {
        EditorAsset* ea = GetEditorAsset(i);
        if (!ea || ea->clipped)
            continue;

        Bounds2 bounds = GetBounds(ea);
        Vec2 p = ea->position + Vec2{(bounds.min.x + bounds.max.x) * 0.5f, GetBounds(ea).min.y};
        Canvas({.type = CANVAS_TYPE_WORLD, .world_camera=g_view.camera, .world_position=p, .world_size={6,0}}, [ea] {
            Align({.alignment=ALIGNMENT_CENTER, .margin=EdgeInsetsTop(16)}, [ea] {
                Label(ea->name->value, {.font = FONT_SEGUISB, .font_size=12, .color=COLOR_WHITE} );
            });
        });
    }
}

void UpdateView()
{
    BeginUI(UI_REF_WIDTH, UI_REF_HEIGHT);
    UpdateViewInternal();
    UpdateCommandPalette();
    UpdateAssetNames();
    UpdateConfirmDialog();
    EndUI();

    BeginRenderFrame(VIEW_COLOR);
    DrawView();
    DrawVfx();
    DrawUI();
    EndRenderFrame();
}


static void HandleFrame()
{
    if (g_view.selected_asset_count <= 0)
        return;

    FrameSelected();
}

static void HandleUIZoomIn()
{
    g_view.ui_scale = Min(g_view.ui_scale + 0.1f, 3.0f);
}

static void HandleUIZoomOut()
{
    g_view.ui_scale = Max(g_view.ui_scale - 0.1f, 0.3f);
}

void HandleRename(const Name* name)
{
    if (g_view.vtable.rename)
        g_view.vtable.rename(name);
}

static void HandleTextInputChange(EventId event_id, const void* event_data)
{
    (void)event_id;

    g_view.command_preview = nullptr;

    if (GetState() == VIEW_STATE_COMMAND) {
        TextInput* input = (TextInput*)event_data;
        if (!ParseCommand(input->value, g_view.command))
            return;

        if (g_view.command.name == NAME_R || g_view.command.name == NAME_RENAME)
            if (g_view.vtable.preview_command)
                g_view.command_preview = g_view.vtable.preview_command(g_view.command);

        return;
    }
}

static void HandleTextInputCancel(EventId event_id, const void* event_data) {
    (void) event_id;
    (void) event_data;

    if (GetState() == VIEW_STATE_COMMAND) {
        EndTextInput();
        PopState();
        return;
    }
}

static void HandleTextInputCommit(EventId event_id, const void* event_data)
{
    (void) event_id;
    (void) event_data;

    if (GetState() == VIEW_STATE_COMMAND) {
        PopState();
        HandleCommand(g_view.command);
        return;
    }
}


void InitViewUserConfig(Props* user_config)
{
    g_view.light_dir = user_config->GetVec2("view", "light_direction", g_view.light_dir);
    SetPosition(g_view.camera, user_config->GetVec2("view", "camera_position", VEC2_ZERO));
    g_view.zoom = user_config->GetFloat("view", "camera_zoom", ZOOM_DEFAULT);
    UpdateCamera();
}

void SaveViewUserConfig(Props* user_config)
{
    user_config->SetVec2("view", "light_direction", g_view.light_dir);
    user_config->SetVec2("view", "camera_position", GetPosition(g_view.camera));
    user_config->SetFloat("view", "camera_zoom", g_view.zoom);
}

static void HandleToggleNames()
{
    g_view.show_names = !g_view.show_names;
}

static void HandleSetDrawModeShaded()
{
    g_view.draw_mode = VIEW_DRAW_MODE_SHADED;
}

static void HandleSetDrawModeWireframe()
{
    g_view.draw_mode = VIEW_DRAW_MODE_WIREFRAME;
}

static void HandleSetDrawModeSolid()
{
    g_view.draw_mode = VIEW_DRAW_MODE_SOLID;
}

static int AssetSortFunc(const void* a, const void* b)
{
    int index_a = *(int*)a;
    int index_b = *(int*)b;

    EditorAsset* ea_a = GetEditorAsset(index_a);
    EditorAsset* ea_b = GetEditorAsset(index_b);
    if (!ea_a && !ea_b)
        return 0;

    if (!ea_a)
        return 1;

    if (!ea_b)
        return 0;

    if (ea_a->sort_order != ea_b->sort_order)
        return ea_a->sort_order - ea_b->sort_order;

    if (ea_a->type != ea_b->type)
        return ea_a->type - ea_b->type;

    return index_a - index_b;
}

static void SortAssets() {
    u32 asset_index = 0;
    for (u32 i=0; i<MAX_ASSETS; i++) {
        EditorAsset* ea = GetEditorAsset(i);
        if (!ea)
            continue;

        g_view.sorted_assets[asset_index++] = i;
    }

    qsort(g_view.sorted_assets, asset_index, sizeof(int), AssetSortFunc);

    asset_index = 0;
    for (u32 i=0, c=GetEditorAssetCount(); i<c; i++) {
        EditorAsset* ea = GetSortedEditorAsset(i);
        if (!ea)
            continue;

        if (ea->sort_order != (int)asset_index * 10)
            ea->meta_modified = true;

        ea->sort_order = asset_index * 10;
        asset_index++;
    }
}

static void HandleSendBack()
{
    if (g_view.selected_asset_count == 0)
        return;

    for (u32 i=0;i<MAX_ASSETS;i++)
    {
        EditorAsset* ea = GetEditorAsset(i);
        if (ea && ea->selected)
            ea->sort_order-=11;
    }

    SortAssets();
}

static void HandleBringForward()
{
    if (g_view.selected_asset_count == 0)
        return;

    for (u32 i=0;i<MAX_ASSETS;i++)
    {
        EditorAsset* ea = GetEditorAsset(i);
        if (ea && ea->selected)
        {
            ea->sort_order += 11;
            ea->meta_modified = true;
        }
    }

    SortAssets();
}

static void HandleBringToFront()
{
    if (g_view.selected_asset_count == 0)
        return;

    for (u32 i=0;i<MAX_ASSETS;i++) {
        EditorAsset* ea = GetEditorAsset(i);
        if (ea && ea->selected) {
            ea->sort_order = 100000;
            ea->meta_modified = true;
        }
    }

    SortAssets();
}

static void HandleSendToBack()
{
    if (g_view.selected_asset_count == 0)
        return;

    for (u32 i=0;i<MAX_ASSETS;i++)
    {
        EditorAsset* ea = GetEditorAsset(i);
        if (ea && ea->selected)
        {
            ea->sort_order = -100000;
            ea->meta_modified = true;
        }
    }

    SortAssets();
}

static void SetCommandState() {
    PushState(VIEW_STATE_COMMAND);
}

static void DeleteSelectedAsset() {
    if (g_view.selected_asset_count == 0)
        return;

    ShowConfirmDialog("Delete asset?", [] {
        for (int i=0; i<MAX_ASSETS; i++) {
            EditorAsset* ea = GetEditorAsset(i);
            if (!ea || !ea->selected) continue;
            DeleteEditorAsset(ea);
        }
        g_view.selected_asset_count=0;
    });
}

void InitView() {
    InitUndo();

    g_view.camera = CreateCamera(ALLOCATOR_DEFAULT);
    g_view.shaded_material = CreateMaterial(ALLOCATOR_DEFAULT, SHADER_LIT);
    g_view.solid_material = CreateMaterial(ALLOCATOR_DEFAULT, SHADER_SOLID);
    g_view.vertex_material = CreateMaterial(ALLOCATOR_DEFAULT, SHADER_UI);
    g_view.editor_material = CreateMaterial(ALLOCATOR_DEFAULT, SHADER_LIT);
    g_view.zoom = ZOOM_DEFAULT;
    g_view.ui_scale = 1.0f;
    g_view.dpi = 72.0f;
    g_view.edit_asset_index = -1;
    g_view.light_dir = { -1, 0 };
    g_view.draw_mode = VIEW_DRAW_MODE_SHADED;

    UpdateCamera();
    SetTexture(g_view.shaded_material, TEXTURE_EDITOR_PALETTE, 0);
    SetTexture(g_view.solid_material, TEXTURE_EDITOR_PALETTE, 0);
    SetTexture(g_view.editor_material, TEXTURE_EDITOR_PALETTE, 0);

    g_view.input = CreateInputSet(ALLOCATOR_DEFAULT);
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

    g_view.input_command = CreateInputSet(ALLOCATOR_DEFAULT);
    EnableButton(g_view.input_command, KEY_ESCAPE);
    EnableButton(g_view.input_command, KEY_ENTER);

    MeshBuilder* builder = CreateMeshBuilder(ALLOCATOR_DEFAULT, 1024, 1024);
    AddCircle(builder, VEC2_ZERO, 0.5f, 8, VEC2_ZERO);
    g_view.vertex_mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE);

    Clear(builder);
    AddVertex(builder, { 0.5f, 0.0f});
    AddVertex(builder, { 0.0f, 0.4f});
    AddVertex(builder, { 0.0f,-0.4f});
    AddTriangle(builder, 0, 1, 2);
    g_view.arrow_mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE);

    Clear(builder);
    AddCircle(builder, VEC2_ZERO, 2.0f, 32, VEC2_ZERO);
    g_view.circle_mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE);

    for (int i=0; i<=100; i++)
    {
        Clear(builder);
        AddArc(builder, VEC2_ZERO, 2.0f, -270, -270 + 360.0f * (i / 100.0f), 32, VEC2_ZERO);
        g_view.arc_mesh[i] = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE);
    }

    Clear(builder);
    AddVertex(builder, { -1, -1});
    AddVertex(builder, {  1, -1});
    AddVertex(builder, {  1,  1});
    AddVertex(builder, { -1,  1});
    AddTriangle(builder, 0, 1, 2);
    AddTriangle(builder, 2, 3, 0);
    g_view.edge_mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE);

    Vec2 bone_collider_verts[] = {
        {0, 0},
        {BONE_WIDTH, -BONE_WIDTH},
        {1, 0},
        {BONE_WIDTH, BONE_WIDTH}
    };
    g_view.bone_collider = CreateCollider(ALLOCATOR_DEFAULT, bone_collider_verts, 4);

    Free(builder);

    InitGrid(ALLOCATOR_DEFAULT);
    InitNotifications();
    LoadEditorAssets();
    SortAssets();
    g_view.state_stack[0] = VIEW_STATE_DEFAULT;
    g_view.state_stack_count = 1;

    Listen(EVENT_TEXTINPUT_CHANGE, HandleTextInputChange);
    Listen(EVENT_TEXTINPUT_CANCEL, HandleTextInputCancel);
    Listen(EVENT_TEXTINPUT_COMMIT, HandleTextInputCommit);

    static Shortcut shortcuts[] = {
        { KEY_F, false, false, false, HandleFrame },
        { KEY_X, false, false, false, DeleteSelectedAsset },
        { KEY_EQUALS, false, true, false, HandleUIZoomIn },
        { KEY_MINUS, false, true, false, HandleUIZoomOut },
        { KEY_N, true, false, false, HandleToggleNames },
        { KEY_1, true, false, false, HandleSetDrawModeWireframe },
        { KEY_2, true, false, false, HandleSetDrawModeSolid },
        { KEY_3, true, false, false, HandleSetDrawModeShaded },
        { KEY_LEFT_BRACKET, false, false, false, HandleSendBack },
        { KEY_RIGHT_BRACKET, false, false, false, HandleBringForward },
        { KEY_RIGHT_BRACKET, false, true, false, HandleBringToFront },
        { KEY_LEFT_BRACKET, false, true, false, HandleSendToBack },
        { KEY_SEMICOLON, false, false, true, SetCommandState },
        { INPUT_CODE_NONE }
    };

    g_view.shortcuts = shortcuts;
    EnableShortcuts(shortcuts);
}


void ShutdownView()
{
    g_view = {};

    Unlisten(EVENT_TEXTINPUT_CHANGE, HandleTextInputChange);
    Unlisten(EVENT_TEXTINPUT_CANCEL, HandleTextInputCancel);

    ShutdownGrid();
    ShutdownWindow();
    ShutdownUndo();
}