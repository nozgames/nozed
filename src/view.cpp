//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "editor.h"
#include "nozed_assets.h"

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
constexpr float FRAME_VIEW_PERCENTAGE = 1.0f / 0.75f;

View g_view = {};

inline ViewState GetState() { return g_view.state; }

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

static Bounds2 GetViewBounds(AssetData* a) {
    if (a == g_editor.editing_asset && g_editor.editing_asset->vtable.editor_bounds)
        return a->vtable.editor_bounds() + a->position;

    return GetBounds(a) + a->position;
}

static void FrameSelected() {
    if (g_view.selected_asset_count == 0)
        return;

    Bounds2 bounds = {};
    bool first = true;

    if (first)
    {
        for (u32 i=0; i<MAX_ASSETS; i++)
        {
            AssetData* ea = GetAssetData(i);
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

static void CommitBoxSelect(const Bounds2& bounds) {
    if (!IsShiftDown(GetInputSet()))
        ClearAssetSelection();

    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* a = GetSortedAssetData(i);
        assert(a);

        if (OverlapBounds(a, bounds))
            SetSelected(a, true);
    }
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

static void UpdateMoveTool(const Vec2& delta) {
    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* ea = GetSortedAssetData(i);
        assert(ea);
        if (!ea->selected)
            continue;

        SetPosition(ea, IsCtrlDown(g_view.input)
            ? SnapToGrid(ea->saved_position + delta)
            : ea->saved_position + delta);
    }
}

static void CommitMoveTool(const Vec2&) {
    EndTextInput();
}

static void CancelMoveTool() {
    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* ea = GetSortedAssetData(i);
        assert(ea);
        if (!ea->selected)
            continue;
        ea->position = ea->saved_position;
    }

    CancelUndo();
    EndTextInput();
}

static void BeginEdit() {
    assert(g_view.state == VIEW_STATE_DEFAULT);

    if (g_view.selected_asset_count != 1)
        return;

    AssetData* a = GetFirstSelectedAsset();
    assert(a);

    if (!a->vtable.editor_begin)
        return;

    g_editor.editing_asset = a;
    a->editing = true;
    SetState(VIEW_STATE_EDIT);
    a->vtable.editor_begin();
}

static void UpdateDefaultState() {
    if (WasButtonPressed(g_view.input, MOUSE_LEFT)) {
        AssetData* hit_asset = HitTestAssets(g_view.mouse_world_position);
        if (hit_asset != nullptr) {
            g_view.clear_selection_on_release = false;
            if (IsShiftDown(g_view.input))
                ToggleSelected(hit_asset);
            else {
                ClearAssetSelection();
                SetSelected(hit_asset, true);
            }
            return;
        }

        g_view.clear_selection_on_release = !IsShiftDown(g_view.input);
    }

    if (g_view.drag_started && g_editor.tool.type == TOOL_TYPE_NONE) {
        BeginBoxSelect(CommitBoxSelect);
        return;
    }

    if (WasButtonReleased(g_view.input, MOUSE_LEFT) && g_view.clear_selection_on_release) {
        ClearAssetSelection();
        return;
    }
}

void SetState(ViewState state) {
    if (state == GetState())
        return;

    switch (g_view.state)
    {
    case VIEW_STATE_EDIT:
        GetAssetData()->editing = false;
        g_editor.editing_asset = nullptr;
        g_view.vtable = {};
        break;

    case VIEW_STATE_COMMAND:
        PopInputSet();
        EndTextInput();
        break;

    default:
        break;
    }

    g_view.state = state;

    switch (state)
    {
    case VIEW_STATE_COMMAND:
        BeginTextInput();
        PushInputSet(g_view.input_command);
        break;

    default:
        break;
    }
}

static void UpdateDrag() {
    g_view.drag_delta = g_view.mouse_position - g_view.drag_position;
    g_view.drag_world_delta = g_view.mouse_world_position - g_view.drag_world_position;
    g_view.drag_started = false;
}

void EndDrag() {
    g_view.drag = false;
    g_view.drag_started = false;
    ConsumeButton(MOUSE_LEFT);
}

void BeginDrag() {
    if (!IsButtonDown(GetInputSet(), MOUSE_LEFT)) {
        g_view.drag_position = g_view.mouse_position;
        g_view.drag_world_position = g_view.mouse_world_position;
    }

    UpdateDrag();

    g_view.drag = true;
    g_view.drag_started = true;
}

static void UpdateMouse() {
    g_view.mouse_position = GetMousePosition();
    g_view.mouse_world_position = ScreenToWorld(g_view.camera, g_view.mouse_position);

    if (g_view.drag) {
        if (WasButtonReleased(GetInputSet(), MOUSE_LEFT))
            EndDrag();
        else
            UpdateDrag();
    } else if (WasButtonPressed(GetInputSet(), MOUSE_LEFT)) {
        g_view.drag_position = g_view.mouse_position;
        g_view.drag_world_position = g_view.mouse_world_position;
    } else if (IsButtonDown(GetInputSet(), MOUSE_LEFT) && Distance(g_view.mouse_position, g_view.drag_position) >= DRAG_MIN) {
        BeginDrag();
    }
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
}

static void UpdateViewInternal() {
    UpdateCamera();
    UpdateMouse();
    UpdateCommon();

    switch (GetState())
    {
    case VIEW_STATE_EDIT:
        // if (WasButtonPressed(g_view.input, KEY_TAB) && !IsAltDown(g_view.input)) {
        //     if (g_view.vtable.shutdown)
        //         g_view.vtable.shutdown();
        //
        //     SetCursor(SYSTEM_CURSOR_DEFAULT);
        //     SetState(VIEW_STATE_DEFAULT);
        //     return;
        // }

        assert(g_editor.editing_asset);
        if (g_editor.editing_asset->vtable.editor_update)
            g_editor.editing_asset->vtable.editor_update();

        break;

    default:
        UpdateDefaultState();
        break;
    }

    if (g_editor.tool.type != TOOL_TYPE_NONE && g_editor.tool.vtable.update)
        g_editor.tool.vtable.update();

    // Save
    if (WasButtonPressed(g_view.input, KEY_S) && IsButtonDown(g_view.input, KEY_LEFT_CTRL))
        SaveEditorAssets();

    UpdateZoom();
    UpdateNotifications();
}

void DrawView() {
    BindCamera(g_view.camera);
    BindLight(Normalize(Vec3{g_view.light_dir.x, g_view.light_dir.y, 0.0f}), COLOR_WHITE, COLOR_BLACK);
    DrawGrid(g_view.camera);

    Bounds2 camera_bounds = GetBounds(g_view.camera);
    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* a = GetSortedAssetData(i);
        assert(a);
        a->clipped = !Intersects(camera_bounds, GetBounds(a) + a->position);
    }

    bool show_names = g_view.show_names || IsAltDown(g_view.input);
    if (show_names) {
        for (u32 i=0, c=GetAssetCount(); i<c; i++) {
            AssetData* a = GetSortedAssetData(i);
            assert(a);
            DrawBounds(a);
        }
    }

    BindColor(COLOR_WHITE);
    BindMaterial(g_view.shaded_material);
    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* a = GetSortedAssetData(i);
        assert(a);
        if (a->clipped)
            continue;

        if (a->editing && a->vtable.editor_draw)
            continue;

        DrawAsset(a);
    }

    if (g_editor.editing_asset && g_editor.editing_asset->vtable.editor_draw)
        g_editor.editing_asset->vtable.editor_draw();

    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* a = GetSortedAssetData(i);
        assert(a);
        if (a->clipped)
            continue;

        DrawOrigin(a);
    }

    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* a = GetAssetData(i);
        assert(a);
        if (!a->selected || a->editing)
            continue;

        DrawBounds(a, 0, COLOR_VERTEX_SELECTED);
    }

    if (IsButtonDown(g_view.input, MOUSE_MIDDLE)) {
        Bounds2 bounds = GetBounds(g_view.camera);
        DrawDashedLine(g_view.mouse_world_position, GetCenter(bounds));
        BindColor(COLOR_VERTEX_SELECTED);
        DrawVertex(g_view.mouse_world_position);
        DrawVertex(GetCenter(bounds));
    }

    if (g_editor.tool.type != TOOL_TYPE_NONE && g_editor.tool.vtable.draw)
        g_editor.tool.vtable.draw();
}

static void UpdateCommandModifier(const TextInput& input) {
    (void) input;
#if 0
    if (GetState() != VIEW_STATE_MODIFIER)
        SetState(VIEW_STATE_MODIFIER);

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
        GetState() != VIEW_STATE_COMMAND)
        return;

    if (!IsAltDown(g_view.input) && !g_view.show_names)
        return;

    for (u32 i=0; i<MAX_ASSETS; i++) {
        AssetData* ea = GetAssetData(i);
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

void UpdateView() {
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
        SetState(VIEW_STATE_DEFAULT);
        return;
    }
}

static void HandleTextInputCommit(EventId event_id, const void* event_data)
{
    (void) event_id;
    (void) event_data;

    if (GetState() == VIEW_STATE_COMMAND) {
        SetState(VIEW_STATE_DEFAULT);
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

static void HandleSendBack()
{
    if (g_view.selected_asset_count == 0)
        return;

    BeginUndoGroup();
    for (u32 i=0, c=GetAssetCount();i<c;i++) {
        AssetData* ea = GetSortedAssetData(i);
        RecordUndo(ea);
        if (!ea->selected)
            continue;

        ea->sort_order-=11;
        ea->meta_modified = true;
    }
    EndUndoGroup();
    SortAssets();
}

static void HandleBringForward()
{
    if (g_view.selected_asset_count == 0)
        return;

    BeginUndoGroup();
    for (u32 i=0, c=GetAssetCount();i<c;i++) {
        AssetData* ea = GetSortedAssetData(i);
        RecordUndo(ea);
        if (!ea->selected)
            continue;

        ea->sort_order += 11;
        ea->meta_modified = true;
    }
    EndUndoGroup();
    SortAssets();
}

static void HandleBringToFront()
{
    if (g_view.selected_asset_count == 0)
        return;

    BeginUndoGroup();
    for (u32 i=0, c=GetAssetCount();i<c;i++) {
        AssetData* ea = GetSortedAssetData(i);
        RecordUndo(ea);
        if (!ea->selected)
            continue;

        ea->sort_order = 100000;
        ea->meta_modified = true;
    }
    EndUndoGroup();
    SortAssets();
}

static void HandleSendToBack()
{
    if (g_view.selected_asset_count == 0)
        return;

    BeginUndoGroup();
    for (u32 i=0, c=GetAssetCount();i<c;i++) {
        AssetData* ea = GetSortedAssetData(i);
        RecordUndo(ea);
        if (!ea->selected)
            continue;

        ea->sort_order = -100000;
        ea->meta_modified = true;
    }
    EndUndoGroup();
    SortAssets();
}

static void SetCommandState() {
    SetState(VIEW_STATE_COMMAND);
}

static void DeleteSelectedAsset() {
    if (g_view.selected_asset_count == 0)
        return;

    ShowConfirmDialog("Delete asset?", [] {
        for (u32 i=GetAssetCount(); i > 0; i--) {
            AssetData* a = GetSortedAssetData(i-1);
            assert(a);
            if (!a->selected) continue;
            RemoveFromUndoRedo(a);
            DeleteAsset(a);
        }
        g_view.selected_asset_count=0;
        SortAssets();
    });
}

void EndEdit() {
    SetState(VIEW_STATE_DEFAULT);
}

void HandleUndo() { Undo(); }
void HandleRedo() { Redo(); }

static void BeginMove() {
    BeginUndoGroup();
    for (u32 i=0, c=GetAssetCount(); i<c; i++)
    {
        AssetData* a = GetSortedAssetData(i);
        assert(a);
        if (!a->selected)
            continue;
        RecordUndo(a);
        a->saved_position = a->position;
    }
    EndUndoGroup();
    BeginTextInput();
    BeginMove({.update=UpdateMoveTool, .commit=CommitMoveTool, .cancel=CancelMoveTool});
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

    g_view.input_tool = CreateInputSet(ALLOCATOR_DEFAULT);
    EnableButton(g_view.input_tool, KEY_ESCAPE);
    EnableButton(g_view.input_tool, KEY_ENTER);
    EnableButton(g_view.input_tool, MOUSE_LEFT);

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
    g_view.state = VIEW_STATE_DEFAULT;

    Listen(EVENT_TEXTINPUT_CHANGE, HandleTextInputChange);
    Listen(EVENT_TEXTINPUT_CANCEL, HandleTextInputCancel);
    Listen(EVENT_TEXTINPUT_COMMIT, HandleTextInputCommit);

    static Shortcut shortcuts[] = {
        { KEY_TAB, false, false, false, BeginEdit },
        { KEY_G, false, false, false, BeginMove },
        { KEY_Z, false, true, false, HandleUndo },
        { KEY_Y, false, true, false, HandleRedo },
        { KEY_F, false, false, false, FrameSelected },
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

    extern void MeshEditorInit();

    MeshEditorInit();
}


void ShutdownView() {
    extern void MeshEditorShutdown();

    MeshEditorShutdown();

    g_view = {};

    Unlisten(EVENT_TEXTINPUT_CHANGE, HandleTextInputChange);
    Unlisten(EVENT_TEXTINPUT_CANCEL, HandleTextInputCancel);

    ShutdownGrid();
    ShutdownWindow();
    ShutdownUndo();
}