//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

enum AnimationViewState {
    ANIMATION_VIEW_STATE_DEFAULT,
    ANIMATION_VIEW_STATE_PLAY,
};

struct AnimationEditor {
    AnimationViewState state;
    bool clear_selection_on_up;
    bool ignore_up;
    Vec2 selection_center;
    Vec2 selection_center_world;
    bool onion_skin;
    InputSet* input;
    Shortcut* shortcuts;
    AnimationFrameData clipboard;
    Vec2 root_motion_delta;
    bool root_motion;
    Animation* playing;
    float play_speed;
};

static AnimationEditor g_animation_editor = {};

static AnimationData* GetAnimationData() {
    AssetData* a = GetAssetData();
    assert(a);
    assert(a->type == ASSET_TYPE_ANIMATION);
    return static_cast<AnimationData*>(a);
}

static SkeletonData* GetSkeletonData() { return GetAnimationData()->skeleton; }

static bool IsBoneSelected(int bone_index) { return GetAnimationData()->bones[bone_index].selected; }

static Vec2 GetRootMotionOffset() {
    AnimationData* n = GetAnimationData();
    return g_animation_editor.root_motion ? VEC2_ZERO : Vec2{-TransformPoint(n->animator->bones[0]).x, 0.0f};
}

static Mat3 GetBaseTransform() {
    AnimationData* n = GetAnimationData();
    return Translate(n->position + GetRootMotionOffset());
}

static bool IsAncestorSelected(int bone_index) {
    AnimationData* n = GetAnimationData();
    SkeletonData* s = GetSkeletonData();
    int parent_index = s->bones[bone_index].parent_index;
    while (parent_index >= 0) {
        if (n->bones[parent_index].selected)
            return true;
        parent_index = s->bones[parent_index].parent_index;
    }

    return false;
}

static void SetBoneSelected(int bone_index, bool selected) {
    if (IsBoneSelected(bone_index) == selected)
        return;

    AnimationData* n = GetAnimationData();
    n->bones[bone_index].selected = selected;
    n->selected_bone_count += selected ? 1 : -1;
}

static void UpdateSelectionCenter() {
    AnimationData* n = GetAnimationData();
    SkeletonData* s = GetSkeletonData();

    Vec2 center = VEC2_ZERO;
    float center_count = 0.0f;
    for (int bone_index=0; bone_index<s->bone_count; bone_index++) {
        if (!IsBoneSelected(bone_index))
            continue;
        center += TransformPoint(n->animator->bones[bone_index]);
        center_count += 1.0f;
    }

    g_animation_editor.selection_center =
        center_count < F32_EPSILON
            ? center
            : center / center_count;
    g_animation_editor.selection_center_world = g_animation_editor.selection_center + n->position + GetRootMotionOffset();
}

static void ClearSelection() {
    SkeletonData* s = GetSkeletonData();
    for (int bone_index=0; bone_index<s->bone_count; bone_index++)
        SetBoneSelected(bone_index, false);
}

static bool TrySelectBone() {
    AnimationData* n = GetAnimationData();

    int hit[MAX_BONES];
    int hit_count = HitTestBones(n, GetBaseTransform(), g_view.mouse_world_position, hit, MAX_BONES);
    if (hit_count == 0) {
        if (!IsShiftDown())
            ClearSelection();
        return false;
    }

    int hit_index=hit_count-1;
    for (; hit_index>=0; hit_index--) {
        int bone_index = hit[hit_index];
        if (IsBoneSelected(bone_index)) {
            hit_index++;
            break;
        }
    }

    if (hit_index < 0 || hit_index >= hit_count)
        hit_index = 0;

    if (!IsShiftDown())
        ClearSelection();

    SetBoneSelected(hit[hit_index], true);

    return true;
}

static void SaveState() {
    AnimationData* n = GetAnimationData();
    SkeletonData* s = GetSkeletonData();
    for (int bone_index=0; bone_index<s->bone_count; bone_index++)
        n->bones[bone_index].saved_transform = GetFrameTransform(n, bone_index, n->current_frame);

    UpdateSelectionCenter();
}

static void RevertToSavedState() {
    AnimationData* n = GetAnimationData();
    SkeletonData* s = GetSkeletonData();
    for (int bone_index=0; bone_index<s->bone_count; bone_index++)
        GetFrameTransform(n, bone_index, n->current_frame) = n->bones[bone_index].saved_transform;

    UpdateTransforms(n);
    UpdateSelectionCenter();
}

static void UpdateBoneNames() {
    if (g_animation_editor.state == ANIMATION_VIEW_STATE_PLAY)
        return;
    
    if (!IsAltDown(g_animation_editor.input) && !g_view.show_names)
        return;

    AnimationData* n = GetAnimationData();
    SkeletonData* s = GetSkeletonData();

    Mat3 base_transform = GetBaseTransform();

    for (u16 bone_index=0; bone_index<s->bone_count; bone_index++) {
        BoneData* b = &s->bones[bone_index];
        Mat3 local_to_world = base_transform * n->animator->bones[bone_index];
        Vec2 p = TransformPoint(local_to_world, Vec2{b->length * 0.5f, 0});
        AnimationBoneData* nb = &n->bones[bone_index];
        BeginCanvas({.type = CANVAS_TYPE_WORLD, .world_camera=g_view.camera, .world_position=p, .world_size={6,1}});
        BeginCenter();
        Label(b->name->value, {.font = FONT_SEGUISB, .font_size=12, .color=nb->selected ? COLOR_VERTEX_SELECTED : COLOR_WHITE} );
        EndCenter();
        EndCanvas();
    }
}

static void UpdatePlayState() {
    assert(g_animation_editor.playing);

    AnimationData* n = GetAnimationData();
    Update(*n->animator, g_animation_editor.play_speed);

    if (!IsPlaying(*n->animator)) {
        Stop(*n->animator);
        Play(*n->animator, g_animation_editor.playing, 0, 1.0f);
    }

    if (g_animation_editor.root_motion)
        g_animation_editor.root_motion_delta.x += n->animator->root_motion_delta;
}

static void HandleBoxSelect(const Bounds2& bounds) {
    if (!IsShiftDown())
        ClearSelection();

    AnimationData* n = GetAnimationData();
    SkeletonData* s = GetSkeletonData();
    Mat3 base_transform = GetBaseTransform();
    for (int bone_index=0; bone_index<s->bone_count; bone_index++) {
        BoneData* b = &s->bones[bone_index];
        Mat3 collider_transform = base_transform * n->animator->bones[bone_index] * Scale(b->length);
        if (OverlapBounds(g_view.bone_collider, collider_transform, bounds))
            SetBoneSelected(bone_index, true);
    }
}

static void UpdateDefaultState() {
    if (!IsToolActive() && g_view.drag_started) {
        BeginBoxSelect(HandleBoxSelect);
        return;
    }

    if (!g_animation_editor.ignore_up && !g_view.drag && WasButtonReleased(g_animation_editor.input, MOUSE_LEFT)) {
        g_animation_editor.clear_selection_on_up = false;
        if (TrySelectBone())
            return;

        g_animation_editor.clear_selection_on_up = true;
    }

    g_animation_editor.ignore_up &= !WasButtonReleased(g_animation_editor.input, MOUSE_LEFT);

    if (WasButtonReleased(g_animation_editor.input, MOUSE_LEFT) && g_animation_editor.clear_selection_on_up && !IsShiftDown())
        ClearSelection();
}

static void SetDefaultState() {
    if (g_animation_editor.state == ANIMATION_VIEW_STATE_DEFAULT)
        return;

    AnimationData* n = GetAnimationData();
    Stop(*n->animator);
    UpdateTransforms(n);

    Free(g_animation_editor.playing);
    g_animation_editor.playing = nullptr;
    g_animation_editor.root_motion_delta = VEC2_ZERO;
    g_animation_editor.state = ANIMATION_VIEW_STATE_DEFAULT;
}

static void ToggleLoop() {
    RecordUndo();
    AnimationData* n = GetAnimationData();
    SetLooping(n, !IsLooping(n));
    MarkModified(n);
}

static void ToggleOnionSkin() {
    g_animation_editor.onion_skin = !g_animation_editor.onion_skin;
}

static void ToggleRootMotion() {
    g_animation_editor.root_motion = !g_animation_editor.root_motion;
    g_animation_editor.root_motion_delta = VEC2_ZERO;
    UpdateSelectionCenter();
    UpdateTransforms(GetAnimationData());
}

static void Mirror() {
    AnimationData* n = GetAnimationData();
    SkeletonData* s = n->skeleton;
    RecordUndo(n);

    Mat3 saved_world_transforms[MAX_BONES];
    for (int bone_index=0; bone_index<s->bone_count; bone_index++)
        saved_world_transforms[bone_index] = n->animator->bones[bone_index];

    for (int bone_index=1; bone_index<s->bone_count; bone_index++) {
        AnimationBoneData* b = &n->bones[bone_index];
        if (!b->selected) continue;

        int mirror_index = GetMirrorBone(s, bone_index);
        if (mirror_index == -1) continue;

        BoneData* bone = &s->bones[bone_index];
        Vec2 desired_world_pos = TransformPoint(saved_world_transforms[mirror_index]);
        float desired_world_rot = GetRotation(saved_world_transforms[mirror_index]);

        Mat3 parent_world = bone->parent_index >= 0
            ? n->animator->bones[bone->parent_index]
            : MAT3_IDENTITY;

        Vec2 local_pos = TransformPoint(Inverse(parent_world), desired_world_pos);
        Vec2 frame_pos = local_pos - bone->transform.position;

        float parent_world_rot = GetRotation(parent_world);
        float frame_rot = desired_world_rot - parent_world_rot - bone->transform.rotation;

        Transform& frame = GetFrameTransform(n, bone_index, n->current_frame);
        SetPosition(frame, frame_pos);
        SetRotation(frame, frame_rot);

        UpdateTransforms(n);
    }

    MarkModified(n);
}

constexpr int   DOPESHEET_MIN_FRAMES = 24;
constexpr float DOPESHEET_FRAME_WIDTH = 20;
constexpr float DOPESHEET_FRAME_HEIGHT = 40;
constexpr float DOPESHEET_PADDING = 8;
constexpr float DOPESHEET_BORDER_WIDTH = 1;
constexpr Color DOPESHEET_BORDER_COLOR = Color8ToColor(10);
constexpr Color DOPESHEET_FRAME_COLOR = Color8ToColor(100);
constexpr float DOPESHEET_FRAME_MARGIN_X = 0; // -DOPESHEET_FRAME_BORDER_WIDTH * 2;
constexpr float DOPESHEET_FRAME_DOT_SIZE = 5;
constexpr float DOPESHEET_FRAME_DOT_OFFSET_X = DOPESHEET_FRAME_WIDTH * 0.5f - DOPESHEET_FRAME_DOT_SIZE * 0.5f;
constexpr float DOPESHEET_FRAME_DOT_OFFSET_Y = 5;
constexpr Color DOPESHEET_FRAME_DOT_COLOR = Color8ToColor(20);
constexpr Color DOPESHEET_FRAME_LOOP_COLOR = Color8ToColor(90);
constexpr Color DOPESHEET_SELECTED_FRAME_COLOR = COLOR_VERTEX_SELECTED;
constexpr Color DOPESHEET_EMPTY_FRAME_COLOR = Color8ToColor(45);
constexpr Color DOPESHEET_TICK_BACKGROUND_COLOR = Color8ToColor(52);
constexpr float DOPESHEET_TICK_WIDTH = DOPESHEET_BORDER_WIDTH;
constexpr float DOPESHEET_TICK_HEIGHT = DOPESHEET_FRAME_HEIGHT * 0.4f;
constexpr Color DOPESHEET_TICK_COLOR = DOPESHEET_BORDER_COLOR;
constexpr Color DOPESHEET_TICK_HOVER_COLOR = Color32ToColor(255,255,255,10);
constexpr float DOPESHEET_SHORT_TICK_HEIGHT = DOPESHEET_TICK_HEIGHT;
constexpr Color DOPESHEET_SHORT_TICK_COLOR = Color8ToColor(44);
constexpr float DOPESHEET_BUTTON_SIZE = DOPESHEET_FRAME_HEIGHT;
constexpr float DOPESHEET_BUTTON_MARGIN_Y = 6;
constexpr float DOPESHEET_BUTTON_SPACING = 8;
constexpr Color DOPESHEET_BUTTON_COLOR = DOPESHEET_FRAME_COLOR;
constexpr Color DOPESHEET_BUTTON_CHECKED_COLOR = COLOR_VERTEX_SELECTED;
constexpr float DOPESHEET_BUTTON_BORDER_WIDTH = 1;
constexpr Color DOPESHEET_BUTTON_BORDER_COLOR = DOPESHEET_BORDER_COLOR;
constexpr Color DOPESHEET_EVENT_COLOR = Color8ToColor(180);

static void DopeSheetButton(Mesh* icon, bool state, void (*on_tap)()) {
    BeginContainer({
        .width=DOPESHEET_BUTTON_SIZE,
        .height=DOPESHEET_BUTTON_SIZE,
        .padding=EdgeInsetsAll(6),
        .color=state ? DOPESHEET_BUTTON_CHECKED_COLOR : DOPESHEET_BUTTON_COLOR,
        .border={.width=DOPESHEET_BUTTON_BORDER_WIDTH, .color=DOPESHEET_BUTTON_BORDER_COLOR}});
    if (WasPressed()) on_tap();
    Image(icon, {.align=ALIGN_CENTER});
    EndContainer();
}

static void DopeSheetFrame(AnimationData* n, int frame_index, int current_frame) {
    AnimationFrameData* f = &n->frames[frame_index];

    BeginContainer({
        .width=DOPESHEET_FRAME_WIDTH + DOPESHEET_FRAME_WIDTH * (f->hold),
        .height=DOPESHEET_FRAME_HEIGHT,
        .margin=EdgeInsetsLeft(DOPESHEET_FRAME_MARGIN_X),
        .color = frame_index == current_frame
            ? DOPESHEET_SELECTED_FRAME_COLOR
            : DOPESHEET_FRAME_COLOR,
        });

    if (IsHovered()) Rectangle({.color=DOPESHEET_TICK_HOVER_COLOR});
    if (WasPressed()) {
        n->current_frame = frame_index;
        UpdateTransforms(n);
        SetDefaultState();
    }

    Container({.width=DOPESHEET_BORDER_WIDTH, .height=DOPESHEET_FRAME_HEIGHT, .color=DOPESHEET_TICK_COLOR});

    // dot
    Container({
        .width=DOPESHEET_FRAME_DOT_SIZE,
        .height=DOPESHEET_FRAME_DOT_SIZE,
        .align=ALIGN_BOTTOM_LEFT,
        .margin=EdgeInsetsBottomLeft(DOPESHEET_FRAME_DOT_OFFSET_Y, DOPESHEET_FRAME_DOT_OFFSET_X),
        .color=DOPESHEET_FRAME_DOT_COLOR});

    EndContainer();
}

static void DopeSheet() {
    AnimationData* n = GetAnimationData();
    int frame_count = Max(GetFrameCountWithHolds(n), DOPESHEET_MIN_FRAMES);

    BeginCanvas();
    BeginContainer({.align=ALIGN_BOTTOM_CENTER, .margin=EdgeInsetsBottom(20)});
    BeginContainer({
        .width=frame_count * DOPESHEET_FRAME_WIDTH + DOPESHEET_PADDING * 2 + 1,
        .align=ALIGN_TOP_CENTER,
        .padding=EdgeInsetsAll(DOPESHEET_PADDING),
        .color=COLOR_UI_BACKGROUND});
    BeginColumn();

    Container({.height=DOPESHEET_BORDER_WIDTH, .color=DOPESHEET_TICK_COLOR});


    // Ticks
    BeginRow();
    bool playing = IsPlaying(*n->animator);
    int current_frame = playing
        ? GetFrameIndex(*n->animator)
        : n->current_frame;
    int last_real_frame_index = -1;
    for (int frame_index=0; frame_index<=frame_count; frame_index++) {
        BeginContainer({
            .width=frame_index == frame_count ? DOPESHEET_TICK_WIDTH : DOPESHEET_FRAME_WIDTH,
            .height=DOPESHEET_TICK_HEIGHT,
            .margin=EdgeInsetsLeft(DOPESHEET_FRAME_MARGIN_X),
            .color=DOPESHEET_TICK_BACKGROUND_COLOR
        });

        int real_frame_index = GetRealFrameIndex(n, frame_index);
        if (WasPressed()) {
            n->current_frame = real_frame_index;
            UpdateTransforms(n);
            SetDefaultState();
        }

        if (IsHovered()) Rectangle({.color=DOPESHEET_TICK_HOVER_COLOR});

        if (real_frame_index < n->frame_count && real_frame_index != last_real_frame_index && n->frames[real_frame_index].event_name != nullptr) {
            BeginContainer({.align=ALIGN_CENTER});
            BeginContainer({.width=DOPESHEET_FRAME_DOT_SIZE * 2, .height=DOPESHEET_FRAME_DOT_SIZE * 2});
            Image(MESH_ASSET_ICON_EVENT, {.color = real_frame_index == current_frame ? COLOR_WHITE : DOPESHEET_EVENT_COLOR});
            EndContainer();
            EndContainer();
        }

        last_real_frame_index = real_frame_index;

        // Tick
        if (frame_index % 4 == 0 || (playing && frame_index == current_frame)) {
            Container({.width=DOPESHEET_BORDER_WIDTH, .color=playing && frame_index == current_frame ? COLOR_WHITE : DOPESHEET_TICK_COLOR});
            // Short Tick
        } else {
            Container({
                .width=DOPESHEET_TICK_WIDTH,
                .height=DOPESHEET_SHORT_TICK_HEIGHT,
                .align=ALIGN_BOTTOM_LEFT,
                .color=DOPESHEET_SHORT_TICK_COLOR
            });
        }
        EndContainer();
    }
    EndRow();

    Container({.height=DOPESHEET_BORDER_WIDTH, .color=DOPESHEET_TICK_COLOR});

    // Frames
    BeginRow();
    {
        int frame_index = 0;
        int frame_index_with_holds = 0;

        current_frame = IsPlaying(*n->animator)
            ? GetRealFrameIndex(n, GetFrameIndex(*n->animator))
            : n->current_frame;
        for (frame_index = 0; frame_index<n->frame_count; frame_index++) {
            AnimationFrameData* f = &n->frames[frame_index];
            frame_index_with_holds += 1 + f->hold;
            DopeSheetFrame(n, frame_index, current_frame);
        }

        // Empty
        for (; frame_index_with_holds<frame_count; frame_index_with_holds++) {
            BeginContainer({
                .width=DOPESHEET_FRAME_WIDTH,
                .height=DOPESHEET_FRAME_HEIGHT,
                .margin=EdgeInsetsLeft(DOPESHEET_FRAME_MARGIN_X),
                .color=DOPESHEET_EMPTY_FRAME_COLOR
            });
            Container({.width=DOPESHEET_BORDER_WIDTH, .height=DOPESHEET_FRAME_HEIGHT, .color=DOPESHEET_TICK_COLOR});
            EndContainer();
        }

        // Right Border
        Container({.width=DOPESHEET_BORDER_WIDTH, .height=DOPESHEET_FRAME_HEIGHT, .color=DOPESHEET_TICK_COLOR});
    }
    EndRow();

    Container({.height=DOPESHEET_BORDER_WIDTH, .color=DOPESHEET_TICK_COLOR});

    Spacer(DOPESHEET_BUTTON_MARGIN_Y);

    // Buttons
    BeginContainer({.height=DOPESHEET_BUTTON_SIZE, .margin=EdgeInsetsLeft(DOPESHEET_FRAME_MARGIN_X)});
    BeginRow({.spacing=DOPESHEET_BUTTON_SPACING});
    {
        DopeSheetButton(MESH_UI_ICON_MIRROR, false, [] { Mirror(); });
        Expanded();
        DopeSheetButton(MESH_UI_ICON_LOOP, IsLooping(n->flags), [] { ToggleLoop(); });
        DopeSheetButton(MESH_UI_ICON_ROOT_MOTION, g_animation_editor.root_motion, [] { ToggleRootMotion(); });
        DopeSheetButton(MESH_UI_ICON_ONION, g_animation_editor.onion_skin, [] { ToggleOnionSkin(); });
    }
    EndRow();
    EndContainer();
    EndColumn();
    EndContainer();
    EndContainer();
    EndCanvas();
}

static void Inspector() {
    AnimationData* n = GetAnimationData();

    BeginInspector();
    BeginInspectorGroup();
    InspectorHeader("Event");

    EventData* events[MAX_ASSETS];

    AnimationFrameData& frame = n->frames[n->current_frame];
    int current_event_index = 0;
    int event_count = 0;;
    for (int asset_index=0, asset_count=GetAssetCount(); asset_index<asset_count; asset_index++) {
        AssetData* a = GetAssetData(asset_index);
        if (a->type != ASSET_TYPE_EVENT) continue;
        if (a->name == frame.event_name)
            current_event_index = event_count + 1;
        events[event_count++] = static_cast<EventData*>(a);
    }

    current_event_index = InspectorRadioButton("None", current_event_index);

    for (int event_index=0; event_index<event_count; event_index++) {
        current_event_index = InspectorRadioButton(events[event_index]->name->value, current_event_index);
    }

    const Name* current_event_name = current_event_index == 0
        ? nullptr
        : events[current_event_index-1]->name;

    if (current_event_name != frame.event_name) {
        RecordUndo(n);
        frame.event_name = current_event_name;
        frame.event = current_event_index == 0 ? nullptr : events[current_event_index-1];
        MarkModified(n);
    }
    EndInspectorGroup();

#if 0
    BeginInspectorGroup();
    InspectorHeader("States");
    EndInspectorGroup();
#endif
    EndInspector();
}

void UpdateAnimationEditor() {
    AnimationData* n = GetAnimationData();
    CheckShortcuts(g_animation_editor.shortcuts, g_animation_editor.input);
    UpdateBounds(n);
    UpdateBoneNames();

    if (g_animation_editor.state == ANIMATION_VIEW_STATE_DEFAULT)
        UpdateDefaultState();
    else if (g_animation_editor.state == ANIMATION_VIEW_STATE_PLAY)
        UpdatePlayState();

    Inspector();
    DopeSheet();
}

static void DrawOnionSkin(int frame) {
    AnimationData* n = GetAnimationData();
    SkeletonData* s = GetSkeletonData();

    UpdateTransforms(n, frame);
    BindSkeleton(&s->bones->world_to_local, sizeof(BoneData), n->animator->bones, sizeof(Mat3), s->bone_count);
    BindTransform(GetBaseTransform());

    for (int skin_index=0; skin_index<s->skin_count; skin_index++) {
        MeshData* skinned_mesh = s->skins[skin_index].mesh;
        if (!skinned_mesh) continue;
        DrawMesh(ToOutlineMesh(skinned_mesh));
    }
}

static void DrawOnionSkin() {
    AnimationData* n = GetAnimationData();
    if (!g_animation_editor.onion_skin || n->frame_count <= 1)
        return;

    BindMaterial(g_view.shaded_skinned_material);

    BindColor(SetAlpha(COLOR_RED, 0.25f));
    DrawOnionSkin((n->current_frame - 1 + n->frame_count) % n->frame_count);

    BindColor(SetAlpha(COLOR_GREEN, 0.25f));
    DrawOnionSkin((n->current_frame + 1) % n->frame_count);

    UpdateTransforms(n);
}

void DrawAnimationEditor() {
    AnimationData* n = GetAnimationData();
    SkeletonData* s = GetSkeletonData();

    Mat3 base_transform = GetBaseTransform() * Translate(g_animation_editor.root_motion_delta);

    BindColor(COLOR_WHITE);
    BindSkeleton(&s->bones[0].world_to_local, sizeof(BoneData), n->animator->bones, sizeof(Mat3), s->bone_count);
    for (int i=0; i<s->skin_count; i++) {
        MeshData* skinned_mesh = s->skins[i].mesh;
        if (!skinned_mesh)
            continue;

        DrawMesh(skinned_mesh, base_transform, g_view.shaded_skinned_material);
    }

    if (g_animation_editor.state == ANIMATION_VIEW_STATE_PLAY)
        return;

    DrawOnionSkin();

    // unselected bones
    BindMaterial(g_view.vertex_material);
    BindColor(COLOR_EDGE);
    for (int bone_index=0; bone_index<s->bone_count; bone_index++) {
        if (IsBoneSelected(bone_index)) continue;
        DrawBone(base_transform * n->animator->bones[bone_index], s->bones[bone_index].length);
    }

    // selected bones
    BindColor(COLOR_EDGE_SELECTED);
    for (int bone_index=0; bone_index<s->bone_count; bone_index++) {
        if (!IsBoneSelected(bone_index)) continue;
        DrawBone(base_transform * n->animator->bones[bone_index], s->bones[bone_index].length);
    }
}

static void HandlePrevFrameCommand() {
    AnimationData* n = GetAnimationData();
    n->current_frame = (n->current_frame - 1 + n->frame_count) % n->frame_count;
    UpdateTransforms(n);
}

static void HandleNextFrameCommand() {
    AnimationData* n = GetAnimationData();
    n->current_frame = (n->current_frame + 1) % n->frame_count;
    UpdateTransforms(n);
}

static void CancelAnimationTool() {
    CancelUndo();
    RevertToSavedState();
}

static void UpdateMoveTool(const Vec2& delta) {
    AnimationData* n = GetAnimationData();
    SkeletonData* s = GetSkeletonData();

    for (int bone_index=0; bone_index<s->bone_count; bone_index++) {
        if (!IsBoneSelected(bone_index) || IsAncestorSelected(bone_index))
            continue;

        Transform& frame = GetFrameTransform(n, bone_index, n->current_frame);
        AnimationBoneData* bone = &n->bones[bone_index];
        int parent_index = s->bones[bone_index].parent_index;
        if (parent_index == -1) {
            SetPosition(frame, bone->saved_transform.position + Vec2{delta.x, 0});
        } else {
            Vec2 rotated_delta = TransformVector(Inverse(n->animator->bones[parent_index]), delta);
            SetPosition(frame, bone->saved_transform.position + rotated_delta);
        }
    }

    UpdateTransforms(n);
}

static void CommitMoveTool(const Vec2&) {
    UpdateTransforms(GetAnimationData());
    MarkModified();
}

static void BeginMoveTool() {
    if (GetAnimationData()->selected_bone_count <= 0)
        return;

    SaveState();
    RecordUndo();
    BeginMoveTool({.update=UpdateMoveTool, .commit=CommitMoveTool, .cancel=CancelAnimationTool});
}

static void UpdateRotateTool(float angle) {
    if (fabsf(angle) < F32_EPSILON)
        return;

    AnimationData* n = GetAnimationData();
    SkeletonData* s = GetSkeletonData();
    for (int bone_index=0; bone_index<s->bone_count; bone_index++) {
        if (!IsBoneSelected(bone_index) || IsAncestorSelected(bone_index))
            continue;

        SetRotation(GetFrameTransform(n, bone_index, n->current_frame), n->bones[bone_index].saved_transform.rotation + angle);
    }

    UpdateTransforms(n);
}

static void CommitRotateTool(float) {
    UpdateTransforms(GetAnimationData());
    MarkModified();
}

static void BeginRotateTool() {
    if (GetAnimationData()->selected_bone_count <= 0)
        return;

    SaveState();
    RecordUndo();
    BeginRotateTool({.origin=g_animation_editor.selection_center_world, .update=UpdateRotateTool, .commit=CommitRotateTool, .cancel=CancelAnimationTool});
}

static void ResetRotate() {
    if (g_animation_editor.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    RecordUndo();
    AnimationData* n = GetAnimationData();
    SkeletonData* s = GetSkeletonData();
    for (int bone_index=0; bone_index<s->bone_count; bone_index++) {
        if (!IsBoneSelected(bone_index))
            continue;

        SetRotation(GetFrameTransform(n, bone_index, n->current_frame), 0);
    }

    MarkModified();
    UpdateTransforms(n);
}

static void PlayAnimation() {
    AnimationData* n = GetAnimationData();
    if (g_animation_editor.state == ANIMATION_VIEW_STATE_PLAY) {
        SetDefaultState();
        return;
    }

    if (g_animation_editor.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    g_animation_editor.playing = ToAnimation(ALLOCATOR_DEFAULT, n);
    g_animation_editor.root_motion_delta = VEC2_ZERO;

    SkeletonData* s = GetSkeletonData();
    Init(*n->animator, ToSkeleton(ALLOCATOR_DEFAULT, s));
    Play(*n->animator, g_animation_editor.playing, 0, 1.0f);

    g_animation_editor.state = ANIMATION_VIEW_STATE_PLAY;
}

static void ResetMove() {
    if (g_animation_editor.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    RecordUndo();

    AnimationData* n = GetAnimationData();
    SkeletonData* s = GetSkeletonData();
    for (int bone_index=0; bone_index<s->bone_count; bone_index++) {
        if (!IsBoneSelected(bone_index))
            continue;

        SetPosition(GetFrameTransform(n, bone_index, n->current_frame), VEC2_ZERO);
    }

    UpdateTransforms(n);
    MarkModified();
}

static void HandleSelectAll() {
    if (g_animation_editor.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    SkeletonData* s = GetSkeletonData();
    for (int i=0; i<s->bone_count; i++)
        SetBoneSelected(i, true);
}

static void InsertFrameBefore() {
    RecordUndo();
    AnimationData* n = GetAnimationData();
    n->current_frame = InsertFrame(n, n->current_frame);
    UpdateTransforms(n);
    MarkModified();
}

static void InsertFrameAfter() {
    RecordUndo();
    AnimationData* n = GetAnimationData();
    n->current_frame = InsertFrame(n, n->current_frame + 1);
    UpdateTransforms(n);
    MarkModified();
}

static void InsertFrameAfterLerp() {
    RecordUndo();
    AnimationData* n = GetAnimationData();
    SkeletonData* s = GetSkeletonData();

    int prev_frame = n->current_frame;
    int new_frame = InsertFrame(n, n->current_frame + 1);
    int next_frame = (new_frame + 1) % n->frame_count;

    // Lerp between current and next frame (next_frame index shifted by 1 due to insert)
    for (int bone_index = 0; bone_index < s->bone_count; bone_index++) {
        Transform& new_transform = GetFrameTransform(n, bone_index, new_frame);
        Transform& next_transform = GetFrameTransform(n, bone_index, next_frame);
        Transform& prev_transform = GetFrameTransform(n, bone_index, prev_frame);
        new_transform = Mix(prev_transform, next_transform, 0.5f);
    }

    n->current_frame = new_frame;
    UpdateTransforms(n);
    MarkModified();
}

static void DeleteFrame() {
    RecordUndo();
    AnimationData* n = GetAnimationData();
    n->current_frame = DeleteFrame(n, n->current_frame);
    UpdateTransforms(n);
    MarkModified();
}

static void AddHoldFrame() {
    AnimationData* n = GetAnimationData();
    RecordUndo();
    n->frames[n->current_frame].hold++;
    MarkModified();
}

static void RemoveHoldFrame() {
    AnimationData* n = GetAnimationData();
    if (n->frames[n->current_frame].hold <= 0)
        return;

    RecordUndo();
    n->frames[n->current_frame].hold = Max(0, n->frames[n->current_frame].hold - 1);
    MarkModified();
}

static void CopyKeys() {
    AnimationData* n = GetAnimationData();
    for (int bone_index=0; bone_index<MAX_BONES; bone_index++)
        g_animation_editor.clipboard.transforms[bone_index] = n->frames[n->current_frame].transforms[bone_index];
}

static void PasteKeys() {
    RecordUndo();

    AnimationData* n = GetAnimationData();
    for (int bone_index=0; bone_index<MAX_BONES; bone_index++) {
        if (!IsBoneSelected(bone_index)) continue;
        n->frames[n->current_frame].transforms[bone_index] = g_animation_editor.clipboard.transforms[bone_index];
    }

    MarkModified();
    UpdateTransforms(n);
}

static void BeginAnimationEditor(AssetData*) {
    ClearSelection();
    SetDefaultState();
    PushInputSet(g_animation_editor.input);
    g_animation_editor.root_motion = true;
    g_animation_editor.root_motion_delta = VEC2_ZERO;
    g_animation_editor.play_speed = 1.0f;
}

static void EndAnimationEditor() {
    SetDefaultState();
    PopInputSet();

    AnimationData* n = GetAnimationData();
    n->current_frame = 0;
    UpdateTransforms(n);
}

static void RootUnitCommand(const Command& command) {
    AnimationData* n = GetAnimationData();
    RecordUndo(n);

    float offset = 1.0f / (n->frame_count - 1);
    if (command.arg_count > 0) {
        offset = (float)atof(command.args[0]);
    }

    for (int frame_index=0; frame_index<n->frame_count; frame_index++) {
        AnimationFrameData* frame = &n->frames[frame_index];
        SetPosition(frame->transforms[0], Vec2{offset * (frame_index + 1), 0.0f});
    }
    MarkModified(n);
    UpdateTransforms(n);
}

static void BeginCommandInput() {
    static CommandHandler commands[] = {
        { NAME_RU, NAME_RU, RootUnitCommand },
        { nullptr, nullptr, nullptr }
    };

    BeginCommandInput({.commands=commands, .prefix=":"});
}

static void CommitUnparentTool(const Vec2& ) {
#if 0
    SkeletonData* s = GetSkeletonData();
    for (int i=0; i<s->skin_count; i++) {
        Skin& sm = s->skinned_meshes[i];
        Vec2 bone_position = TransformPoint(s->bones[sm.bone_index].local_to_world) + s->position;
        if (!sm.mesh || !OverlapPoint(sm.mesh, bone_position, position))
            continue;

        RecordUndo(s);
        for (int j=i; j<s->skin_count-1; j++)
            s->skinned_meshes[j] = s->skinned_meshes[j+1];

        s->skin_count--;

        MarkModified();
        return;
    }
#endif
}

static void BeginUnparentTool() {
    BeginSelectTool({.commit=CommitUnparentTool});
}

static void CommitParentTool(const Vec2& position) {
    AssetData* hit_asset = HitTestAssets(position);
    if (!hit_asset || hit_asset->type != ASSET_TYPE_MESH)
        return;

    RecordUndo();
    // s->skinned_meshes[s->skinned_mesh_count++] = {
    //     hit_asset->name,
    //     (MeshData*)hit_asset,
    //     GetFirstSelectedBoneIndex()
    // };

    MarkModified();
}

static void BeginParentTool() {
    BeginSelectTool({.commit=CommitParentTool});
}

static void IncPlaySpeed() {
    g_animation_editor.play_speed = Min(g_animation_editor.play_speed + 0.1f, 4.0f);
}

static void DecPlaySpeed() {
    g_animation_editor.play_speed = Max(g_animation_editor.play_speed - 0.1f, 0.1f);
}


void InitAnimationEditor() {
    g_animation_editor = {};

    static Shortcut shortcuts[] = {
        { KEY_SEMICOLON, false, false, true, BeginCommandInput },
        { KEY_G, false, false, false, BeginMoveTool },
        { KEY_R, false, false, false, BeginRotateTool },
        { KEY_R, true, false, false, ResetRotate },
        { KEY_G, true, false, false, ResetMove },
        { KEY_A, false, false, false, HandleSelectAll },
        { KEY_Q, false, false, false, HandlePrevFrameCommand },
        { KEY_E, false, false, false, HandleNextFrameCommand },
        { KEY_SPACE, false, false, false, PlayAnimation },
        { KEY_I, false, false, false, InsertFrameBefore },
        { KEY_O, false, false, false, InsertFrameAfter },
        { KEY_X, false, false, false, DeleteFrame },
        { KEY_H, false, false, false, AddHoldFrame },
        { KEY_H, false, true, false, RemoveHoldFrame },
        { KEY_O, true, false, false, ToggleOnionSkin },
        { KEY_O, false, false, true, InsertFrameAfterLerp },
        { KEY_C, false, true, false, CopyKeys },
        { KEY_V, false, true, false, PasteKeys },
        { KEY_M, true, false, false, ToggleRootMotion },
        { KEY_P, false, false, false, BeginParentTool },
        { KEY_P, false, true, false, BeginUnparentTool },
        { KEY_LEFT, false, false, false, DecPlaySpeed },
        { KEY_RIGHT, false, false, false, IncPlaySpeed },

        { INPUT_CODE_NONE }
    };

    g_animation_editor.shortcuts = shortcuts;
    g_animation_editor.input = CreateInputSet(ALLOCATOR_DEFAULT);
    EnableButton(g_animation_editor.input, MOUSE_LEFT);
    EnableModifiers(g_animation_editor.input);
    EnableShortcuts(shortcuts, g_animation_editor.input);
    EnableCommonShortcuts(g_animation_editor.input);
}

void InitAnimationEditor(AnimationData* n) {
    n->vtable.editor_begin = BeginAnimationEditor;
    n->vtable.editor_end = EndAnimationEditor;
    n->vtable.editor_draw = DrawAnimationEditor;
    n->vtable.editor_update = UpdateAnimationEditor;
}
