//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

struct SkeletonEditor {
    void (*state_update)();
    void (*state_draw)();
    bool clear_selection_on_up;
    bool ignore_up;
    Vec2 command_world_position;
    Vec2 selection_drag_start;
    Vec2 selection_center;
    Vec2 selection_center_world;
    Shortcut* shortcuts;
    InputSet* input;
    BoneData saved_bones[MAX_BONES];};

static SkeletonEditor g_skeleton_editor = {};

inline SkeletonData* GetSkeletonData() {
    AssetData* ea = GetAssetData();
    assert(ea);
    assert(ea->type == ASSET_TYPE_SKELETON);
    return (SkeletonData*)ea;
}

static bool IsBoneSelected(int bone_index) { return GetSkeletonData()->bones[bone_index].selected; }
static bool IsAncestorSelected(int bone_index) {
    SkeletonData* es = GetSkeletonData();
    int parent_index = es->bones[bone_index].parent_index;
    while (parent_index >= 0) {
        if (es->bones[parent_index].selected)
            return true;
        parent_index = es->bones[parent_index].parent_index;
    }

    return false;
}

static void SetBoneSelected(int bone_index, bool selected) {
    if (IsBoneSelected(bone_index) == selected)
        return;
    SkeletonData* es = GetSkeletonData();
    es->bones[bone_index].selected = selected;
    es->selected_bone_count += selected ? 1 : -1;
}

static int GetFirstSelectedBoneIndex() {
    SkeletonData* es = GetSkeletonData();
    for (int i=0; i<es->bone_count; i++)
        if (IsBoneSelected(i))
            return i;
    return -1;
}

static void UpdateAllAnimations(SkeletonData* s) {
    extern void UpdateSkeleton(AnimationData* en);

    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AnimationData* a = static_cast<AnimationData*>(GetAssetData(i));
        if (a->type != ASSET_TYPE_ANIMATION)
            continue;

        if (s != a->skeleton)
            continue;

        RecordUndo(a);
        UpdateSkeleton(a);
        MarkModified(a);
    }
}

static void UpdateBoneNames() {
    if (!IsAltDown(g_skeleton_editor.input) && !g_view.show_names)
        return;

    SkeletonData* s = GetSkeletonData();
    for (u16 i=0; i<s->bone_count; i++) {
        BoneData* b = s->bones + i;
        const Mat3& transform = b->local_to_world;
        Vec2 p = (TransformPoint(Translate(s->position) * transform, Vec2{b->length * 0.5f, }));
        Canvas({.type = CANVAS_TYPE_WORLD, .world_camera=g_view.camera, .world_position=p, .world_size={6,1}}, [b] {
            Align({.alignment=ALIGNMENT_CENTER_CENTER}, [b] {
                Label(b->name->value, {.font = FONT_SEGUISB, .font_size=12, .color=b->selected ? COLOR_VERTEX_SELECTED : COLOR_WHITE} );
            });
        });
    }
}

static void UpdateSelectionCenter() {
    SkeletonData* s = GetSkeletonData();
    Vec2 center = VEC2_ZERO;
    float center_count = 0.0f;
    for (int i=0; i<s->bone_count; i++) {
        BoneData& eb = s->bones[i];
        if (!IsBoneSelected(i))
            continue;
        center += TransformPoint(eb.local_to_world);
        center_count += 1.0f;
    }

    g_skeleton_editor.selection_center =
        center_count < F32_EPSILON
            ? center
            : center / center_count;
    g_skeleton_editor.selection_center_world = g_skeleton_editor.selection_center + s->position;
}

static void SaveState() {
    SkeletonData* s = GetSkeletonData();
    for (int bone_index=0; bone_index<s->bone_count; bone_index++)
        g_skeleton_editor.saved_bones[bone_index] = s->bones[bone_index];
}

static void RevertToSavedState() {
    SkeletonData* s = GetSkeletonData();
    for (int bone_index=0; bone_index<s->bone_count; bone_index++)
        s->bones[bone_index] = g_skeleton_editor.saved_bones[bone_index];

    UpdateTransforms(s);
    UpdateSelectionCenter();
}

static void ClearSelection() {
    SkeletonData* es = GetSkeletonData();
    for (int bone_index=0; bone_index<es->bone_count; bone_index++)
        SetBoneSelected(bone_index, false);
}

static bool TrySelect() {
    SkeletonData* es = GetSkeletonData();
    int bone_index = HitTestBone(es, g_view.mouse_world_position);
    if (bone_index == -1)
        return false;

    BoneData* eb = &es->bones[bone_index];
    if (IsShiftDown(g_skeleton_editor.input)) {
        SetBoneSelected(bone_index, !eb->selected);
    } else {
        ClearSelection();
        SetBoneSelected(bone_index, true);
    }

    return true;
}

static void HandleBoxSelect(const Bounds2& bounds) {
    if (!IsShiftDown(g_skeleton_editor.input))
        ClearSelection();

    SkeletonData* s = GetSkeletonData();
    for (int bone_index=0; bone_index<s->bone_count; bone_index++) {
        BoneData* b = &s->bones[bone_index];
        Mat3 collider_transform =
            Translate(s->position) *
            b->local_to_world *
            Rotate(b->transform.rotation) *
            Scale(b->length);
        if (OverlapBounds(g_view.bone_collider, collider_transform, bounds))
            SetBoneSelected(bone_index, true);
    }
}

static void UpdateDefaultState() {
    if (!IsToolActive() && g_view.drag_started) {
        BeginBoxSelect(HandleBoxSelect);
        return;
    }

    if (!g_skeleton_editor.ignore_up && !g_view.drag && WasButtonReleased(g_skeleton_editor.input, MOUSE_LEFT)) {
        g_skeleton_editor.clear_selection_on_up = false;

        if (TrySelect())
            return;

        g_skeleton_editor.clear_selection_on_up = true;
    }

    g_skeleton_editor.ignore_up &= !WasButtonReleased(g_skeleton_editor.input, MOUSE_LEFT);

    if (WasButtonReleased(g_skeleton_editor.input, MOUSE_LEFT) && g_skeleton_editor.clear_selection_on_up) {
        ClearSelection();
    }
}

void UpdateSkeletonEditor() {
    CheckShortcuts(g_skeleton_editor.shortcuts, g_skeleton_editor.input);
    UpdateBoneNames();

    if (g_skeleton_editor.state_update)
        g_skeleton_editor.state_update();

    UpdateDefaultState();
}

static void DrawSkeleton() {
    AssetData* ea = GetAssetData();
    SkeletonData* es = GetSkeletonData();

    DrawSkeletonData(es, ea->position);

    // Draw selected bones in front
    BindMaterial(g_view.vertex_material);
    BindColor(COLOR_BONE_SELECTED);
    for (int bone_index=0; bone_index<es->bone_count; bone_index++) {
        if (!IsBoneSelected(bone_index))
            continue;

        DrawEditorSkeletonBone(es, bone_index, ea->position);
    }
}

void DrawSkeletonData() {
    DrawBounds(GetSkeletonData(), 0, COLOR_BLACK);
    DrawSkeleton();

    if (g_skeleton_editor.state_draw)
        g_skeleton_editor.state_draw();
}

static void CancelSkeletonTool() {
    CancelUndo();
    RevertToSavedState();
}

static void UpdateMoveTool(const Vec2& delta) {
    SkeletonData* s = GetSkeletonData();
    for (int bone_index=0; bone_index<s->bone_count; bone_index++) {
        if (!IsBoneSelected(bone_index) || IsAncestorSelected(bone_index))
            continue;

        BoneData& b = s->bones[bone_index];
        BoneData& p = bone_index >= 0 ? s->bones[b.parent_index] : s->bones[0];
        BoneData& sb = g_skeleton_editor.saved_bones[bone_index];

        b.transform.position = TransformPoint(p.world_to_local, TransformPoint(sb.local_to_world) + delta);
    }

    UpdateTransforms(s);
}

static void CommitMoveTool(const Vec2&) {
    SkeletonData* s = GetSkeletonData();
    UpdateTransforms(s);
    MarkModified();
}

static void BeginMoveTool(bool record_undo) {
    if (GetSkeletonData()->selected_bone_count <= 0)
        return;

    SaveState();
    if (record_undo)
        RecordUndo();

    SetCursor(SYSTEM_CURSOR_MOVE);
    BeginMoveTool({.update=UpdateMoveTool, .commit=CommitMoveTool, .cancel=CancelSkeletonTool});
}

static void BeginMoveTool() {
    BeginMoveTool(true);
}

static void UpdateRotateTool(float angle) {
    SkeletonData* s = GetSkeletonData();

    for (int bone_index=0; bone_index<s->bone_count; bone_index++) {
        if (!IsBoneSelected(bone_index))
            continue;

        BoneData& b = s->bones[bone_index];
        BoneData& sb = g_skeleton_editor.saved_bones[bone_index];
        if (IsCtrlDown())
            b.transform.rotation = SnapAngle(sb.transform.rotation + angle);
        else
            b.transform.rotation = sb.transform.rotation + angle;
    }

    UpdateTransforms(s);
    MarkModified();
}

static void BeginRotateTool() {
    SkeletonData* s = GetSkeletonData();
    if (s->selected_bone_count <= 0)
        return;

    UpdateSelectionCenter();
    SaveState();
    RecordUndo();
    BeginRotateTool({.origin=g_skeleton_editor.selection_center_world, .update=UpdateRotateTool, .cancel=CancelSkeletonTool});
}

static void UpdateScaleTool(float scale) {
    SkeletonData* s = GetSkeletonData();
    for (i32 bone_index=0; bone_index<s->bone_count; bone_index++) {
        if (!IsBoneSelected(bone_index))
            continue;

        BoneData& b = s->bones[bone_index];
        BoneData& sb = g_skeleton_editor.saved_bones[bone_index];
        b.length = Clamp(sb.length * scale, 0.05f, 10.0f);
    }

    UpdateTransforms(s);
}

static void BeginScaleTool() {
    SkeletonData* s = GetSkeletonData();
    if (s->selected_bone_count <= 0)
        return;

    UpdateSelectionCenter();
    SaveState();
    RecordUndo();
    BeginScaleTool({.origin=g_skeleton_editor.selection_center_world, .update=UpdateScaleTool, .cancel=CancelSkeletonTool});
}

static void HandleRemove() {
    SkeletonData* s = GetSkeletonData();
    if (s->selected_bone_count <= 0)
        return;

    BeginUndoGroup();
    RecordUndo();

    for (int i=s->bone_count - 1; i >=0; i--) {
        if (!IsBoneSelected(i))
            continue;

        RemoveBone(s, i);
    }

    UpdateAllAnimations(s);
    EndUndoGroup();
    ClearSelection();
    MarkModified();
}

static void CommitParentTool(const Vec2& position) {
    SkeletonData* s = GetSkeletonData();
    int bone_index = HitTestBone(s, position);
    if (bone_index != -1) {
        BeginUndoGroup();
        RecordUndo(s);
        bone_index = ReparentBone(s, GetFirstSelectedBoneIndex(), bone_index);
        ClearSelection();
        SetBoneSelected(bone_index, true);
        UpdateAllAnimations(s);
        EndUndoGroup();
        return;
    }

    AssetData* hit_asset = HitTestAssets(position);
    if (!hit_asset || hit_asset->type != ASSET_TYPE_MESH)
        return;

    RecordUndo();
    s->skins[s->skin_count++] = {
        .asset_name = hit_asset->name,
        .mesh = (MeshData*)hit_asset,
    };
    UpdateTransforms(s);

    MarkModified();
}

static void BeginParentTool() {
    BeginSelectTool({.commit=CommitParentTool});
}

static void CommitUnparentTool(const Vec2& position) {
    SkeletonData* s = GetSkeletonData();
    for (int i=0; i<s->skin_count; i++) {
        Skin& sm = s->skins[i];
        if (!sm.mesh || !OverlapPoint(sm.mesh, s->position, position))
            continue;

        RecordUndo(s);
        for (int j=i; j<s->skin_count-1; j++)
            s->skins[j] = s->skins[j+1];

        s->skin_count--;

        MarkModified();
        return;
    }
}

static void BeginUnparentTool() {
    BeginSelectTool({.commit=CommitUnparentTool});
}

static void BeginExtrudeTool() {
    SkeletonData* s = GetSkeletonData();
    if (s->selected_bone_count != 1)
        return;

    if (s->bone_count >= MAX_BONES)
        return;

    int parent_bone_index = GetFirstSelectedBoneIndex();
    assert(parent_bone_index != -1);

    BoneData& parent_bone = s->bones[parent_bone_index];

    RecordUndo();

    s->bones[s->bone_count] = {
        .name = GetUniqueBoneName(s),
        .index = s->bone_count,
        .parent_index = parent_bone_index,
        .transform = { .scale = VEC2_ONE },
        .length = parent_bone.length
    };
    s->bone_count++;

    UpdateTransforms(s);
    ClearSelection();
    SetBoneSelected(s->bone_count-1, true);
    BeginMoveTool(false);
}

static void RenameBoneCommand(const Command& command) {
    if (command.arg_count != 0)
        return;

    SkeletonData* s = GetSkeletonData();
    if (s->selected_bone_count != 1) {
        LogError("can only rename a single selected bone");
        return;
    }

    MarkModified();
    BeginUndoGroup();
    RecordUndo();
    s->bones[GetFirstSelectedBoneIndex()].name = command.name;
    UpdateAllAnimations(s);
    EndUndoGroup();
}

static void BeginRenameCommand() {
    static CommandHandler commands[] = {
        {NAME_NONE, NAME_NONE, RenameBoneCommand},
        {nullptr, nullptr, nullptr}
    };

    int bone_index = GetFirstSelectedBoneIndex();
    if (bone_index == -1)
        return;

    BeginCommandInput({
        .commands = commands,
        .placeholder = GetSkeletonData()->bones[bone_index].name->value
    });
}

static void BeginSkeletonEditor(AssetData*) {
    PushInputSet(g_skeleton_editor.input);
    ClearSelection();
}

static void EndSkeletonEditor() {
    PopInputSet();
}

static void ResetRotation() {
    SkeletonData* s = GetSkeletonData();
    RecordUndo(s);
    for (int bone_index=1; bone_index<s->bone_count; bone_index++) {
        if (!IsBoneSelected(bone_index))
            continue;

        BoneData& b = s->bones[bone_index];
        b.transform.rotation = 0;
    }

    UpdateTransforms(s);
    MarkModified(s);
}

static void ResetTranslation() {
    SkeletonData* s = GetSkeletonData();
    RecordUndo(s);

    if (IsBoneSelected(0)) {
        BoneData& bone = s->bones[0];
        bone.transform.position = VEC2_ZERO;
    }

    for (int bone_index=1; bone_index<s->bone_count; bone_index++) {
        if (!IsBoneSelected(bone_index))
            continue;

        BoneData& b = s->bones[bone_index];
        BoneData& p = s->bones[b.parent_index];
        b.transform.position = Vec2{p.length, 0};
    }

    UpdateTransforms(s);
    MarkModified(s);
}

void InitSkeletonEditor(SkeletonData* s) {
    s->vtable.editor_begin = BeginSkeletonEditor;
    s->vtable.editor_end = EndSkeletonEditor;
    s->vtable.editor_draw = DrawSkeletonData;
    s->vtable.editor_update = UpdateSkeletonEditor;
}

void InitSkeletonEditor() {
    static Shortcut shortcuts[] = {
        { KEY_G, false, false, false, BeginMoveTool },
        { KEY_P, false, false, false, BeginParentTool },
        { KEY_P, false, true, false, BeginUnparentTool },
        { KEY_E, false, true, false, BeginExtrudeTool },
        { KEY_R, false, false, false, BeginRotateTool },
        { KEY_X, false, false, false, HandleRemove },
        { KEY_S, false, false, false, BeginScaleTool },
        { KEY_F2, false, false, false, BeginRenameCommand },
        { KEY_R, true, false, false, ResetRotation },
        { KEY_G, true, false, false, ResetTranslation },
        { INPUT_CODE_NONE }
    };

    g_skeleton_editor.input = CreateInputSet(ALLOCATOR_DEFAULT);
    EnableButton(g_skeleton_editor.input, MOUSE_LEFT);
    EnableButton(g_skeleton_editor.input, KEY_LEFT_SHIFT);
    EnableButton(g_skeleton_editor.input, KEY_RIGHT_SHIFT);
    EnableButton(g_skeleton_editor.input, MOUSE_SCROLL_Y);

    g_skeleton_editor.shortcuts = shortcuts;
    EnableShortcuts(g_skeleton_editor.shortcuts, g_skeleton_editor.input);
    EnableCommonShortcuts(g_skeleton_editor.input);
}
