//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

constexpr float FRAME_SIZE_X = 20;
constexpr float FRAME_SIZE_Y = 40;
constexpr float FRAME_BORDER_SIZE = 1;
constexpr Color FRAME_BORDER_COLOR = Color24ToColor(32,32,32);
constexpr float FRAME_DOT_SIZE = 5;
constexpr float FRAME_DOT_OFFSET_X = FRAME_SIZE_X * 0.5f - FRAME_DOT_SIZE * 0.5f;
constexpr float FRAME_DOT_OFFSET_Y = 5;
constexpr Color FRAME_DOT_COLOR = FRAME_BORDER_COLOR;
constexpr Color FRAME_COLOR = Color32ToColor(100, 100, 100, 255);
constexpr Color FRAME_SELECTED_COLOR = COLOR_VERTEX_SELECTED;

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

static int HitTestBone(AnimationData* n, const Vec2& world_pos) {
    SkeletonData* s = n->skeleton;
    UpdateTransforms(n);

    int first_hit_index = -1;
    Mat3 base_transform = GetBaseTransform();
    for (int bone_index=s->bone_count-1; bone_index>=0; bone_index--) {
        AnimationBoneData* b = &n->bones[bone_index];
        BoneData* sb = &s->bones[bone_index];
        Mat3 local_to_world = base_transform * n->animator->bones[bone_index] * Scale(sb->length);

        // Mat3 local_to_world =
        //     base_transform *
        //     n->animator->bones[bone_index] *
        //     Rotate(s->bones[bone_index].transform.rotation) *
        //     Scale(sb->length);

        if (OverlapPoint(g_view.bone_collider, local_to_world, world_pos)) {
            if (first_hit_index == -1)
                first_hit_index = bone_index;
            if (!b->selected) {
                return bone_index;
            }
        }
    }

    return first_hit_index;
}

static bool TrySelectBone() {
    AnimationData* n = GetAnimationData();
    int bone_index = HitTestBone(n, g_view.mouse_world_position);
    if (bone_index == -1)
        return false;

    if (IsShiftDown()) {
        SetBoneSelected(bone_index, !IsBoneSelected(bone_index));
    } else {
        ClearSelection();
        SetBoneSelected(bone_index, true);
    }

    return true;
}

static bool TrySelectMesh() {
    AnimationData* n = GetAnimationData();
    SkeletonData* s = GetSkeletonData();
    for (int i=s->skin_count-1; i>=0; i--) {
        MeshData* skinned_mesh = s->skins[i].mesh;
        if (!skinned_mesh)
            continue;

        Mat3 mesh_transform = Translate(n->position) * n->animator->bones[s->skins[i].bone_index];

        int face_index = HitTestFace(
            skinned_mesh,
            mesh_transform,
            g_view.mouse_world_position);

        if (face_index == -1)
            continue;

        int bone_index = s->skins[i].bone_index;
        BoneData* b = &s->bones[bone_index];
        if (IsShiftDown(g_animation_editor.input)) {
            SetBoneSelected(bone_index, !b->selected);
        } else {
            ClearSelection();
            SetBoneSelected(bone_index, true);
        }

        return true;
    }

    return false;
}

static bool TrySelect() {
    if (TrySelectBone())
        return true;

    return TrySelectMesh();
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
        Mat3 local_to_world =
            base_transform *
            n->animator->bones[bone_index] *
            Rotate(s->bones[bone_index].transform.rotation);

        Vec2 p = TransformPoint(local_to_world, Vec2{b->length * 0.5f, 0});
        AnimationBoneData* nb = &n->bones[bone_index];
        Canvas({.type = CANVAS_TYPE_WORLD, .world_camera=g_view.camera, .world_position=p, .world_size={6,1}}, [b,nb] {
            Align({.alignment=ALIGNMENT_CENTER_CENTER}, [b,nb] {
                Label(b->name->value, {.font = FONT_SEGUISB, .font_size=12, .color=nb->selected ? COLOR_VERTEX_SELECTED : COLOR_WHITE} );
            });
        });
    }
}

static void UpdatePlayState() {
    AnimationData* n = GetAnimationData();
    if (!n->animation)
        n->animation = ToAnimation(ALLOCATOR_DEFAULT, n);

    if (!n->animation)
        return;

    Update(*n->animator, 1.0f);

    if (!IsPlaying(*n->animator)) {
        Stop(*n->animator);
        Play(*n->animator, n->animation, 0, 1.0f);
    }

    if (g_animation_editor.root_motion)
        g_animation_editor.root_motion_delta += n->animator->root_motion_delta;
}

static void HandleBoxSelect(const Bounds2& bounds) {
    if (!IsShiftDown())
        ClearSelection();

    AnimationData* n = GetAnimationData();
    SkeletonData* s = GetSkeletonData();
    for (int bone_index=0; bone_index<s->bone_count; bone_index++) {
        BoneData* eb = &s->bones[bone_index];
        Mat3 collider_transform =
            Translate(n->position) *
            n->animator->bones[bone_index] *
            Rotate(eb->transform.rotation) *
            Scale(eb->length);
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
        if (TrySelect())
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
    if (IsPlaying(*n->animator)) {
        Stop(*n->animator);
        g_animation_editor.root_motion_delta = VEC2_ZERO;
    }

    g_animation_editor.root_motion_delta = VEC2_ZERO;

    UpdateTransforms(n);

    g_animation_editor.state = ANIMATION_VIEW_STATE_DEFAULT;
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

    Canvas([n] {
        Align({.alignment=ALIGNMENT_BOTTOM_CENTER, .margin=EdgeInsetsBottom(20)}, [n] {
            Row([n] {
                Container({
                    .width=FRAME_SIZE_Y + FRAME_BORDER_SIZE * 2,
                    .height=FRAME_SIZE_Y + FRAME_BORDER_SIZE * 2,
                    .margin=EdgeInsetsLeftRight(-FRAME_SIZE_X, FRAME_SIZE_X),
                    .padding=EdgeInsetsAll(4),
                    .color=FRAME_COLOR,
                    .border = {.width=FRAME_BORDER_SIZE, .color=FRAME_BORDER_COLOR}}, [] {
                    Image(g_animation_editor.root_motion ? MESH_UI_ICON_ROOT_MOTION : MESH_UI_ICON_ROOT_MOTION_OFF);
                });

                int current_frame = n->current_frame;
                for (int frame_index=0; frame_index<n->frame_count; frame_index++) {
                    AnimationFrameData* f = &n->frames[frame_index];
                    Container({
                        .width=FRAME_SIZE_X * (1 + f->hold) + FRAME_BORDER_SIZE * 2,
                        .height=FRAME_SIZE_Y + FRAME_BORDER_SIZE * 2,
                        .margin=EdgeInsetsLeft(-2),
                        .color = frame_index == current_frame
                            ? FRAME_SELECTED_COLOR
                            : FRAME_COLOR,
                        .border = {.width=FRAME_BORDER_SIZE, .color=FRAME_BORDER_COLOR}
                    },
                    [] {
                        Align({.alignment=ALIGNMENT_BOTTOM_LEFT, .margin=EdgeInsetsBottomLeft(FRAME_DOT_OFFSET_Y, FRAME_DOT_OFFSET_X)}, [] {
                            Container({.width=FRAME_DOT_SIZE, .height=FRAME_DOT_SIZE, .color=FRAME_DOT_COLOR});
                        });
                    });
                }
            });
        });
    });
}

static void DrawOnionSkin() {
    AnimationData* n = GetAnimationData();
    SkeletonData* s = GetSkeletonData();

    if (!g_animation_editor.onion_skin || n->frame_count <= 1)
        return;

    int frame = n->current_frame;

    n->current_frame = (frame - 1 + n->frame_count) % n->frame_count;
    UpdateTransforms(n);

    BindMaterial(g_view.vertex_material);
    BindColor(SetAlpha(COLOR_RED, 0.25f));
    for (int bone_index=0; bone_index<s->bone_count; bone_index++) {
        BoneData* eb = &s->bones[bone_index];
        DrawBone(
            n->animator->bones[bone_index] * Rotate(eb->transform.rotation),
            eb->parent_index < 0
                ? n->animator->bones[bone_index]
                : n->animator->bones[eb->parent_index],
            n->position,
            eb->length
            );
    }

    n->current_frame = (frame + 1 + n->frame_count) % n->frame_count;
    UpdateTransforms(n);

    BindColor(SetAlpha(COLOR_GREEN, 0.25f));
    for (int bone_index=0; bone_index<s->bone_count; bone_index++) {
        BoneData* eb = &s->bones[bone_index];
        DrawBone(
            n->animator->bones[bone_index] * Rotate(eb->transform.rotation),
            eb->parent_index < 0
                ? n->animator->bones[bone_index]
                : n->animator->bones[eb->parent_index],
            n->position,
            eb->length);
    }

    n->current_frame = frame;
    UpdateTransforms(n);
}

void DrawAnimationEditor() {
    AnimationData* n = GetAnimationData();
    SkeletonData* s = GetSkeletonData();

    BindColor(COLOR_WHITE);
    Mat3 base_transform = GetBaseTransform() * Translate(g_animation_editor.root_motion_delta);

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

    Vec2 base_position = TransformPoint(base_transform);
    BindMaterial(g_view.vertex_material);
    BindColor(COLOR_EDGE);
    for (int bone_index=0; bone_index<s->bone_count; bone_index++)
        DrawEditorAnimationBone(n, bone_index, base_position);

    BindColor(COLOR_EDGE_SELECTED);
    for (int bone_index=0; bone_index<s->bone_count; bone_index++) {
        if (!IsBoneSelected(bone_index))
            continue;

        DrawEditorAnimationBone(n, bone_index, base_position);
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
            SetPosition(frame, bone->saved_transform.position + delta);
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

    g_animation_editor.root_motion_delta = VEC2_ZERO;

    SkeletonData* s = GetSkeletonData();
    Init(*n->animator, ToSkeleton(ALLOCATOR_DEFAULT, s));
    Play(*n->animator, ToAnimation(ALLOCATOR_DEFAULT, n), 0, 1.0f);

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
    // todo: lerp between current and next frame
    n->current_frame = InsertFrame(n, n->current_frame + 1);
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

static void ToggleOnionSkin() {
    g_animation_editor.onion_skin = !g_animation_editor.onion_skin;
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
        if (!IsBoneSelected(bone_index))
            continue;
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
}

static void EndAnimationEditor() {
    SetDefaultState();
    PopInputSet();

    AnimationData* n = GetAnimationData();
    n->current_frame = 0;
    UpdateTransforms(n);
}

static void ToggleRootMotion() {
    g_animation_editor.root_motion = !g_animation_editor.root_motion;
    g_animation_editor.root_motion_delta = VEC2_ZERO;
    UpdateSelectionCenter();
    UpdateTransforms(GetAnimationData());
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
        SetPosition(frame->transforms[0], Vec2{offset * frame_index, frame->transforms[0].position.y});
    }
    MarkModified(n);
    UpdateTransforms(n);
}

static void Mirror(const Command&) {
    AnimationData* n = GetAnimationData();
    RecordUndo(n);

    // Save the original world transforms
    Mat3 saved_transforms[MAX_BONES];
    for (int bone_index=0; bone_index<n->bone_count; bone_index++)
        saved_transforms[bone_index] = n->animator->bones[bone_index];

    // Process bones in hierarchy order (parents first, index 0 is root)
    // After each bone, update transforms so children see the new parent position
    for (int bone_index=1; bone_index<n->skeleton->bone_count; bone_index++) {
        AnimationBoneData* b = &n->bones[bone_index];
        if (!b->selected)
            continue;

        int mirror = GetMirrorBone(n->skeleton, bone_index);
        if (mirror == -1)
            continue;

        // Target is exactly where the mirror bone was (swap positions)
        Vec2 target_world_pos = TransformPoint(saved_transforms[mirror]);

        BoneData* sb = &n->skeleton->bones[bone_index];
        Mat3& parent_transform = n->animator->bones[sb->parent_index];
        Vec2 frame_pos = TransformPoint(Inverse(parent_transform), target_world_pos);

        Transform& frame_transform = GetFrameTransform(n, bone_index, n->current_frame);
        SetPosition(frame_transform, frame_pos - sb->transform.position);

        // Get world rotation of mirror bone (including its skeleton bone rotation)
        BoneData* mirror_sb = &n->skeleton->bones[mirror];
        float mirror_world_rotation = Angle(GetForward(saved_transforms[mirror])) + mirror_sb->transform.rotation;

        // Convert to local rotation for target bone
        // Target visual rotation = parent_rotation + frame.rotation + skeleton_bone.rotation
        // So: frame.rotation = mirror_world_rotation - parent_rotation - skeleton_bone.rotation
        float parent_world_rotation = Angle(GetForward(parent_transform));
        SetRotation(frame_transform, mirror_world_rotation - parent_world_rotation - sb->transform.rotation);

        // Update transforms so child bones see the new parent position
        UpdateTransforms(n);
    }

    MarkModified(n);
    UpdateTransforms(n);
}

static void BeginCommandInput() {
    static CommandHandler commands[] = {
        { NAME_RU, NAME_RU, RootUnitCommand },
        { NAME_MIRROR, NAME_MIRROR, Mirror },
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
