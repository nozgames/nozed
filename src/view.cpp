//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

extern void InitMeshEditor();
extern void InitTextureEditor();
extern void InitSkeletonEditor();
extern void InitAnimationEditor();
extern void InitAnimatedMeshEditor();

constexpr float SELECT_SIZE = 60.0f;
constexpr float DRAG_MIN = 5;
constexpr float DEFAULT_DPI = 72.0f;
constexpr float ZOOM_MIN = 0.1f;
constexpr float ZOOM_MAX = 200.0f;
constexpr float ZOOM_STEP = 0.1f;
constexpr float ZOOM_DEFAULT = 1.0f;
constexpr float VERTEX_SIZE = 0.1f;
constexpr Color VERTEX_COLOR = { 0.95f, 0.95f, 0.95f, 1.0f};
constexpr Color VIEW_COLOR = {0.05f, 0.05f, 0.05f, 1.0f};
constexpr float FRAME_VIEW_PERCENTAGE = 1.0f / 0.75f;

View g_view = {};

inline ViewState GetState() { return g_view.state; }
static void CheckCommonShortcuts();

static void UpdateCamera() {
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

    if (first) {
        for (u32 i=0, c=GetAssetCount(); i<c; i++) {
            AssetData* ea = GetAssetData(i);
            assert(ea);
            if (!ea->selected)
                continue;

            if (first)
                bounds = GetViewBounds(ea);
            else
                bounds = Union(bounds, GetViewBounds(ea));

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
    g_view.zoom_version++;

    SetPosition(g_view.camera, center);
    UpdateCamera();
}

static void CommitBoxSelect(const Bounds2& bounds) {
    if (!IsShiftDown())
        ClearAssetSelection();

    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* a = GetAssetData(i);
        assert(a);

        if (OverlapBounds(a, bounds))
            SetSelected(a, true);
    }
}

static void UpdatePanState() {
    if (WasButtonPressed(GetInputSet(), MOUSE_RIGHT)) {
        g_view.pan_position = g_view.mouse_position;
        g_view.pan_position_camera = GetPosition(g_view.camera);
    }

    if (IsButtonDown(GetInputSet(), MOUSE_RIGHT)) {
        Vec2 delta = g_view.mouse_position - g_view.pan_position;
        Vec2 world_delta = ScreenToWorld(g_view.camera, delta) - ScreenToWorld(g_view.camera, VEC2_ZERO);
        SetPosition(g_view.camera, g_view.pan_position_camera - world_delta);
    }
}

static void UpdateZoom() {
    float zoom_axis = GetAxis(GetInputSet(), MOUSE_SCROLL_Y);
    if (zoom_axis > -0.5f && zoom_axis < 0.5f)
        return;

    Vec2 mouse_screen = GetMousePosition();
    Vec2 world_under_cursor = ScreenToWorld(g_view.camera, mouse_screen);

    f32 zoom_factor = 1.0f + zoom_axis * ZOOM_STEP;
    g_view.zoom *= zoom_factor;
    g_view.zoom = Clamp(g_view.zoom, ZOOM_MIN, ZOOM_MAX);
    g_view.zoom_version++;

    UpdateCamera();

    Vec2 world_under_cursor_after = ScreenToWorld(g_view.camera, mouse_screen);
    Vec2 current_position = GetPosition(g_view.camera);
    Vec2 world_offset = world_under_cursor - world_under_cursor_after;
    SetPosition(g_view.camera, current_position + world_offset);
}

static void UpdateMoveTool(const Vec2& delta) {
    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* a = GetAssetData(i);
        assert(a);
        if (!a->selected)
            continue;

        SetPosition(a, IsCtrlDown(GetInputSet())
            ? SnapToGrid(a->saved_position + delta)
            : a->saved_position + delta);
    }
}

static void CancelMoveTool() {
    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* a = GetAssetData(i);
        assert(a);
        if (!a->selected)
            continue;
        a->position = a->saved_position;
    }

    CancelUndo();
}

static void ToggleEdit() {
    if (g_view.state == VIEW_STATE_EDIT) {
        EndEdit();
        return;
    }

    if (g_view.selected_asset_count != 1)
        return;

    AssetData* a = GetFirstSelectedAsset();
    assert(a);

    if (!a->vtable.editor_begin)
        return;

    g_editor.editing_asset = a;
    a->editing = true;
    SetState(VIEW_STATE_EDIT);
    a->vtable.editor_begin(a);
}

static void UpdateDefaultState() {
    CheckShortcuts(g_view.shortcuts);

    if (WasButtonPressed(g_view.input, MOUSE_LEFT)) {
        AssetData* hit_asset = HitTestAssets(g_view.mouse_world_position);
        if (hit_asset != nullptr) {
            g_view.clear_selection_on_release = false;
            if (IsShiftDown())
                ToggleSelected(hit_asset);
            else {
                ClearAssetSelection();
                SetSelected(hit_asset, true);
            }
            return;
        }

        g_view.clear_selection_on_release = !IsShiftDown();
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

    switch (g_view.state) {
    case VIEW_STATE_EDIT:
        GetAssetData()->editing = false;
        g_editor.editing_asset = nullptr;
        g_view.vtable = {};
        break;

    default:
        break;
    }

    g_view.state = state;
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

static void UpdateCommon() {
    CheckCommonShortcuts();
    UpdateCamera();
    UpdateMouse();
    UpdatePanState();

    if (IsButtonDown(g_view.input, MOUSE_MIDDLE)) {
        Vec2 dir = Normalize(GetScreenCenter() - g_view.mouse_position);
        g_view.light_dir = Vec2{-dir.x, dir.y};
    }
}

static void UpdateViewInternal() {
    UpdateCommon();

    switch (GetState()) {
    case VIEW_STATE_EDIT:
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

    UpdateZoom();
    UpdateNotifications();
}

void DrawView() {
    BindCamera(g_view.camera);

    if (g_view.grid)
        DrawGrid(g_view.camera);

    Bounds2 camera_bounds = GetBounds(g_view.camera);
    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* a = GetAssetData(i);
        assert(a);
        a->clipped = !Intersects(camera_bounds, GetBounds(a) + a->position);
    }

    bool show_names = g_view.state == VIEW_STATE_DEFAULT && (g_view.show_names || IsAltDown(g_view.input));
    if (show_names) {
        for (u32 i=0, c=GetAssetCount(); i<c; i++) {
            AssetData* a = GetAssetData(i);
            if (a->clipped)
                continue;
            DrawBounds(a);
        }
    }

    BindColor(COLOR_WHITE);
    BindMaterial(g_view.shaded_material);
    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* a = GetAssetData(i);
        assert(a);
        if (a->clipped)
            continue;

        if (a->editing && a->vtable.editor_draw)
            continue;

        DrawAsset(a);
    }

    if (g_view.state == VIEW_STATE_EDIT && g_editor.editing_asset)
        DrawBounds(g_editor.editing_asset, 0, COLOR_EDGE);

    if (g_editor.editing_asset && g_editor.editing_asset->vtable.editor_draw)
        g_editor.editing_asset->vtable.editor_draw();

    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* a = GetAssetData(i);
        assert(a);
        if (a->clipped)
            continue;

        if (!g_editor.editing_asset && a->selected) {
            DrawBounds(a, 0, COLOR_VERTEX_SELECTED);
            DrawOrigin(a);
        }
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

static void UpdateAssetNames() {
    if (GetState() != VIEW_STATE_DEFAULT &&
        GetState() != VIEW_STATE_COMMAND)
        return;

    if (!IsAltDown(g_view.input) && !g_view.show_names)
        return;

    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* a = GetAssetData(i);
        if (a->clipped)
            continue;

        Bounds2 bounds = GetBounds(a);
        Vec2 p = a->position + Vec2{(bounds.min.x + bounds.max.x) * 0.5f, GetBounds(a).min.y};
        Canvas({.type = CANVAS_TYPE_WORLD, .world_camera=g_view.camera, .world_position=p, .world_size={6,0}}, [a] {
            Align({.alignment=ALIGNMENT_CENTER_CENTER, .margin=EdgeInsetsTop(16)}, [a] {
                Label(a->name->value, {.font = FONT_SEGUISB, .font_size=12, .color=a->selected ? COLOR_VERTEX_SELECTED : COLOR_WHITE} );
            });
        });
    }
}

void UpdateView() {
    BeginUI(UI_REF_WIDTH, UI_REF_HEIGHT);
    UpdateViewInternal();
    UpdateCommandInput();
    UpdateAssetNames();
    UpdateConfirmDialog();
    EndUI();

    BeginRenderFrame(VIEW_COLOR);
    DrawView();
    DrawVfx();
    DrawUI();
    EndRenderFrame();
}

void InitViewUserConfig(Props* user_config){
    SetPosition(g_view.camera, user_config->GetVec2("view", "camera_position", VEC2_ZERO));
    g_view.zoom = user_config->GetFloat("view", "camera_zoom", ZOOM_DEFAULT);
    g_view.show_names = user_config->GetBool("view", "show_names", false);
    g_view.grid = user_config->GetBool("view", "show_grid", true);
    UpdateCamera();
}

void SaveViewUserConfig(Props* user_config) {
    user_config->SetVec2("view", "camera_position", GetPosition(g_view.camera));
    user_config->SetFloat("view", "camera_zoom", g_view.zoom);
    user_config->SetBool("view", "show_names", g_view.show_names);
    user_config->SetBool("view", "show_grid", g_view.grid);
}

static void ToggleNames() {
    g_view.show_names = !g_view.show_names;
}

static void ToggleModeWireframe() {
    g_view.draw_mode = g_view.draw_mode == VIEW_DRAW_MODE_SHADED
        ? VIEW_DRAW_MODE_WIREFRAME
        : VIEW_DRAW_MODE_SHADED;
}

static void BringForward() {
    if (g_view.selected_asset_count == 0)
        return;

    BeginUndoGroup();
    for (u32 i=0, c=GetAssetCount();i<c;i++) {
        AssetData* a = GetAssetData(i);
        RecordUndo(a);
        if (!a->selected)
            continue;

        if (a->type == ASSET_TYPE_MESH) {
            MeshData* m = static_cast<MeshData*>(a);
            m->depth = Clamp(m->depth+1, MIN_DEPTH, MAX_DEPTH);
            MarkDirty(m);
            MarkModified(a);
        }
    }
    EndUndoGroup();
    SortAssets();
}

static void BringToFront() {
    if (g_view.selected_asset_count == 0)
        return;

    BeginUndoGroup();
    for (u32 i=0, c=GetAssetCount();i<c;i++) {
        AssetData* a = GetAssetData(i);
        RecordUndo(a);
        if (!a->selected)
            continue;

        MarkMetaModified(a);
    }
    EndUndoGroup();
    SortAssets();
}

static void SendBackward() {
    if (g_view.selected_asset_count == 0)
        return;

    BeginUndoGroup();
    for (u32 i=0, c=GetAssetCount();i<c;i++) {
        AssetData* a = GetAssetData(i);
        RecordUndo(a);
        if (!a->selected)
            continue;

        if (a->type == ASSET_TYPE_MESH) {
            MeshData* m = static_cast<MeshData*>(a);
            m->depth = Clamp(m->depth-1, MIN_DEPTH, MAX_DEPTH);
            MarkDirty(m);
            MarkModified(a);
        }
    }
    EndUndoGroup();
    SortAssets();
}

static void SendToBack() {
    if (g_view.selected_asset_count == 0)
        return;

    BeginUndoGroup();
    for (u32 i=0, c=GetAssetCount();i<c;i++) {
        AssetData* a = GetAssetData(i);
        RecordUndo(a);
        if (!a->selected)
            continue;

        MarkModified(a);
    }
    EndUndoGroup();
    SortAssets();
}

static void DeleteSelectedAssets() {
    if (g_view.selected_asset_count == 0)
        return;

    ShowConfirmDialog("Delete asset?", [] {
        AssetData* selected[MAX_ASSETS];
        int selected_count = GetSelectedAssets(selected, MAX_ASSETS);
        for (int i=0; i<selected_count; i++) {
            AssetData* a = selected[i];
            RemoveFromUndoRedo(a);
            DeleteAsset(a);
        }
        g_view.selected_asset_count=0;
        SortAssets();
    });
}

void EndEdit() {
    AssetData* a = GetAssetData();
    assert(a);
    if (a->vtable.editor_end)
        a->vtable.editor_end();

    SetCursor(SYSTEM_CURSOR_DEFAULT);
    SetState(VIEW_STATE_DEFAULT);
}

void HandleUndo() { Undo(); }
void HandleRedo() { Redo(); }

static void BeginMoveTool() {
    BeginUndoGroup();
    for (u32 i=0, c=GetAssetCount(); i<c; i++)
    {
        AssetData* a = GetAssetData(i);
        assert(a);
        if (!a->selected)
            continue;
        RecordUndo(a);
        a->saved_position = a->position;
    }
    EndUndoGroup();
    BeginMoveTool({.update=UpdateMoveTool, .cancel=CancelMoveTool});
}

// @command
static void SaveAssetsCommand(const Command& command) {
    (void)command;
    SaveAssetData();
}

static void NewAssetCommand(const Command& command) {
    if (command.arg_count < 1) {
        LogError("missing asset type (mesh, etc)");
        return;
    }

    const Name* type = GetName(command.args[0]);
    if (command.arg_count < 2) {
        LogError("missing asset name");
        return;
    }

    const Name* asset_name = GetName(command.args[1]);

    AssetData* a = nullptr;
    if (type == NAME_MESH || type == NAME_M)
        a = NewMeshData(asset_name->value);
    else if (type == NAME_SKELETON || type == NAME_S)
        a = NewEditorSkeleton(asset_name->value);
    else if (type == NAME_ANIMATION || type == NAME_A)
        a = NewAnimationData(asset_name->value);
    else if (type == NAME_VFX)
        a = NewVfxData(asset_name->value);
    else if (type == NAME_AM || type == NAME_ANIMATEDMESH)
        a = NewAnimatedMeshData(asset_name->value);
    else if (type == NAME_EVENT || type == NAME_E)
        a = NewEventData(asset_name->value);

    if (a == nullptr)
        return;

    a->position = GetCenter(GetBounds(g_view.camera));
    MarkMetaModified(a);

    if (a->vtable.post_load)
        a->vtable.post_load(a);

    SortAssets();
    SaveAssetData();
}

static void DuplicateAsset() {
    if (g_view.selected_asset_count == 0) {
        AddNotification(NOTIFICATION_TYPE_ERROR, "NO ASSET SELECTED");
        return;
    }

    AssetData* selected[MAX_ASSETS];
    int selected_count = GetSelectedAssets(selected, MAX_ASSETS);

    ClearAssetSelection();

    for (int i=0; i<selected_count; i++) {
        AssetData* a = selected[i];
        AssetData* d = Duplicate(a);
        if (!d) {
            AddNotification(NOTIFICATION_TYPE_ERROR, "DUPLICATE FAILED");
            continue;
        }

        d->position = a->position + Vec2{0.5f, -0.5f};
        SetSelected(d, true);
    }

    SaveAssetData();
    BeginMoveTool();
}

static void BuildAssets(const Command&) {
    Build();
}

static void BeginCommandInput() {
    static CommandHandler commands[] = {
        { NAME_S, NAME_SAVE, SaveAssetsCommand },
        { NAME_N, NAME_NEW, NewAssetCommand },
        { NAME_B, NAME_BUILD, BuildAssets },
        { nullptr, nullptr, nullptr }
    };

    BeginCommandInput({.commands=commands, .prefix=":"});
}

static void SetPalette(u32 index) {
    if (index >= g_view.palette_count)
        return;

    g_view.active_palette_index = index;
    Texture* palette_texture = g_view.palettes[g_view.active_palette_index].texture->texture;
    SetTexture(g_view.editor_material, palette_texture);
    SetTexture(g_view.shaded_material, palette_texture);
    SetTexture(g_view.shaded_skinned_material, palette_texture);

    extern void UpdateMeshEditorPalette();

    UpdateMeshEditorPalette();
}

static void PrevPalette() {
    SetPalette((g_view.active_palette_index + g_view.palette_count - 1) % g_view.palette_count);
}

static void NextPalette() {
    SetPalette((g_view.active_palette_index + 1) % g_view.palette_count);
}

static void HandleRenameCommand(const Command& command) {
    AssetData* a = GetFirstSelectedAsset();
    if (!a)
        return;

    if (!command.name) {
        AddNotification(NOTIFICATION_TYPE_ERROR, "invalid name");
        return;
    }

    MarkModified(a);
    RecordUndo(a);
    if (!Rename(a, command.name))
        AddNotification(NOTIFICATION_TYPE_ERROR, "rename failed");
}

static void RenameAsset() {
    static CommandHandler commands[] = {
        {NAME_NONE, NAME_NONE, HandleRenameCommand},
        {nullptr, nullptr, nullptr}
    };

    if (g_view.selected_asset_count != 1)
        return;

    AssetData* a = GetFirstSelectedAsset();
    BeginCommandInput({
        .commands = commands,
        .placeholder = a->name->value
    });
}

static void PlayAsset() {
    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* a = GetAssetData(i);
        assert(a);
        if (!a->selected)
            continue;

        if (a->vtable.play)
            a->vtable.play(a);
    }
}

// @shortcut
static Shortcut g_common_shortcuts[] = {
    { KEY_S, false, true, false, SaveAssetData },
    { KEY_F, false, false, false, FrameSelected },
    { KEY_N, true, false, false, ToggleNames },
    { KEY_W, true, false, false, ToggleModeWireframe },
    { KEY_Z, false, true, false, HandleUndo },
    { KEY_Y, false, true, false, HandleRedo },
    { KEY_TAB, false, false, false, ToggleEdit },
    { KEY_COMMA, false, false, false, PrevPalette },
    { KEY_PERIOD, false, false, false, NextPalette },
    { INPUT_CODE_NONE }
};


void EnableCommonShortcuts(InputSet* input_set) {
    EnableShortcuts(g_common_shortcuts, input_set);
    EnableModifiers(input_set);
    EnableButton(input_set, MOUSE_RIGHT);
}

void CheckCommonShortcuts() {
    CheckShortcuts(g_common_shortcuts, GetInputSet());
}

static void DrawSetOriginTool(const Vec2& position) {
    Vec2 snapped_position = position;
    if (IsCtrlDown())
        snapped_position = SnapToGrid(position);

    BindColor(COLOR_WHITE);
    DrawVertex(snapped_position, -0.2f);
}

static void CommitSetOriginTool(const Vec2& position) {
    Vec2 snapped_position = position;
    if (IsCtrlDown())
        snapped_position = SnapToGrid(position);

    BeginUndoGroup();

    AssetData* selected[MAX_ASSETS];
    int selected_count = GetSelectedAssets(selected, MAX_ASSETS);
    for (int i=0; i<selected_count; i++) {
        AssetData* a = selected[i];
        if (a->type == ASSET_TYPE_MESH) {
            RecordUndo(a);
            SetOrigin(static_cast<MeshData*>(a), snapped_position);
            MarkModified(a);
        }
    }

    EndUndoGroup();
}

void BeginSetOriginTool() {
    AssetData* selected[MAX_ASSETS];
    int selected_count = GetSelectedAssets(selected, MAX_ASSETS);
    int viable_count = 0;
    for (int i=0; i<selected_count; i++) {
        AssetData* a = selected[i];
        if (a->type == ASSET_TYPE_MESH)
            viable_count++;
    }

    if (!viable_count)
        return;

    BeginSelectTool({
        .commit = CommitSetOriginTool,
        .draw = DrawSetOriginTool
    });
}

static void ToggleGrid() {
    g_view.grid = !g_view.grid;
}

void InitView() {
    InitUndo();

    g_view.camera = CreateCamera(ALLOCATOR_DEFAULT);
    g_view.shaded_material = CreateMaterial(ALLOCATOR_DEFAULT, SHADER_MESH);
    g_view.vertex_material = CreateMaterial(ALLOCATOR_DEFAULT, SHADER_UI);
    g_view.editor_material = CreateMaterial(ALLOCATOR_DEFAULT, SHADER_MESH);
    g_view.shaded_skinned_material = CreateMaterial(ALLOCATOR_DEFAULT, SHADER_SKINNED_MESH);
    g_view.zoom = ZOOM_DEFAULT;
    g_view.ui_scale = 1.0f;
    g_view.dpi = 72.0f;
    g_view.light_dir = { -1, 0 };
    g_view.draw_mode = VIEW_DRAW_MODE_SHADED;

    UpdateCamera();

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
    EnableCommonShortcuts(g_view.input);
    PushInputSet(g_view.input);

    g_view.input_tool = CreateInputSet(ALLOCATOR_DEFAULT, GetName("tool"));
    EnableModifiers(g_view.input_tool);
    EnableButton(g_view.input_tool, KEY_ESCAPE);
    EnableButton(g_view.input_tool, KEY_ENTER);
    EnableButton(g_view.input_tool, MOUSE_LEFT);
    EnableButton(g_view.input_tool, MOUSE_RIGHT);

    MeshBuilder* builder = CreateMeshBuilder(ALLOCATOR_DEFAULT, 1024, 1024);
    AddCircle(builder, VEC2_ZERO, 0.5f, 8, ColorUV(7,0));
    g_view.vertex_mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE);

    Clear(builder);
    AddVertex(builder, Vec2{ 0.5f, 0.0f});
    AddVertex(builder, Vec2{ 0.0f, 0.4f});
    AddVertex(builder, Vec2{ 0.0f,-0.4f});
    AddTriangle(builder, 0, 1, 2);
    g_view.arrow_mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE);

    Clear(builder);
    AddCircle(builder, VEC2_ZERO, 2.0f, 32, VEC2_ZERO);
    g_view.circle_mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE);

    Clear(builder);
    AddCircleStroke(builder, VEC2_ZERO, 2.0f, 0.4f, 32, VEC2_ZERO);
    g_view.circle_stroke_mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE);

    for (int i=0; i<=100; i++) {
        Clear(builder);
        AddArc(builder, VEC2_ZERO, 2.0f, -270, -270 + 360.0f * (i / 100.0f), 32, VEC2_ZERO);
        g_view.arc_mesh[i] = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE);
    }

    Clear(builder);
    AddVertex(builder, Vec2{ -1, -1});
    AddVertex(builder, Vec2{  1, -1});
    AddVertex(builder, Vec2{  1,  1});
    AddVertex(builder, Vec2{ -1,  1});
    AddTriangle(builder, 0, 1, 2);
    AddTriangle(builder, 2, 3, 0);
    g_view.edge_mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE);

    Clear(builder);
    AddVertex(builder, Vec2{ -0.5f, -0.5f}, Vec2{0,1});
    AddVertex(builder, Vec2{  0.5f, -0.5f}, Vec2{1,1});
    AddVertex(builder, Vec2{  0.5f,  0.5f}, Vec2{1,0});
    AddVertex(builder, Vec2{ -0.5f,  0.5f}, Vec2{0,0});
    AddTriangle(builder, 0, 1, 2);
    AddTriangle(builder, 2, 3, 0);
    g_view.quad_mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE);

    Vec2 bone_collider_verts[] = {
        {0, 0},
        {BONE_WIDTH, -BONE_WIDTH},
        {1, 0},
        {BONE_WIDTH, BONE_WIDTH}
    };
    g_view.bone_collider = CreateCollider(ALLOCATOR_DEFAULT, bone_collider_verts, 4);

    Free(builder);

    InitGrid(ALLOCATOR_DEFAULT);
    g_view.state = VIEW_STATE_DEFAULT;

    static Shortcut shortcuts[] = {
        { KEY_G, false, false, false, BeginMoveTool },
        { KEY_X, false, false, false, DeleteSelectedAssets },
        { KEY_D, false, true, false, DuplicateAsset },
        { KEY_O, false, false, true, BeginSetOriginTool },
        { KEY_LEFT_BRACKET, false, false, false, SendBackward },
        { KEY_RIGHT_BRACKET, false, false, false, BringForward },
        { KEY_RIGHT_BRACKET, false, true, false, BringToFront },
        { KEY_LEFT_BRACKET, false, true, false, SendToBack },
        { KEY_SEMICOLON, false, false, true, BeginCommandInput },
        { KEY_F2, false, false, false, RenameAsset },
        { KEY_SPACE, false, false, false, PlayAsset },
        { KEY_QUOTE, true, false, false, ToggleGrid },
        { INPUT_CODE_NONE }
    };

    g_view.shortcuts = shortcuts;
    EnableShortcuts(shortcuts);

    for (auto& palette_key : g_config->GetKeys("palettes")) {
        std::string palette_value = g_config->GetString("palettes", palette_key.c_str(), nullptr);

        Tokenizer tk;
        Init(tk, palette_value.c_str());
        Token name_token;
        float uv_x = 0.0f;
        float uv_y = 0.0f;
        ExpectToken(tk, &name_token);
        const Name* palette_name = GetName(tk);
        if (ExpectDelimiter(tk, ','))
            ExpectFloat(tk, &uv_x);
        if (ExpectDelimiter(tk, ','))
            ExpectFloat(tk, &uv_y);

        TextureData* palette_texture = static_cast<TextureData*>(GetAssetData(ASSET_TYPE_TEXTURE, palette_name));
        if (!palette_texture)
            continue;

        g_view.palettes[g_view.palette_count++] = {
            .texture = palette_texture,
            .color_offset_uv = Vec2{uv_x, uv_y}
        };
    }

    InitMeshEditor();
    InitTextureEditor();
    InitSkeletonEditor();
    InitAnimationEditor();
    InitAnimatedMeshEditor();

    SetPalette(0);
}


void ShutdownView() {
    extern void ShutdownMeshEditor();

    ShutdownMeshEditor();

    g_view = {};

    ShutdownGrid();
    ShutdownWindow();
    ShutdownUndo();
}