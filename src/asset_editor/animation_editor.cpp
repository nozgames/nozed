//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"

constexpr float FRAME_LINE_SIZE = 0.5f;
constexpr float FRAME_LINE_OFFSET = -0.1f;
constexpr float FRAME_SIZE = 0.16f;
constexpr float FRAME_SELECTED_SIZE = 0.32f;

constexpr float CENTER_SIZE = 0.2f;
constexpr float ORIGIN_SIZE = 0.1f;
constexpr float ORIGIN_BORDER_SIZE = 0.12f;
constexpr float ROTATE_TOOL_WIDTH = 0.02f;

constexpr float BONE_ORIGIN_SIZE = 0.16f;

enum AnimationEditorState
{
    ANIMATION_EDITOR_STATE_DEFAULT,
    ANIMATION_EDITOR_STATE_MOVE,
    ANIMATION_EDITOR_STATE_ROTATE,
    ANIMATION_EDITOR_STATE_PLAY,
};

struct SavedBone
{
    Mat3 world_to_local;
    Vec2 world_position;
    BoneTransform transform;
};

struct AnimationEditor
{
    AnimationEditorState state;
    EditorAsset* asset;
    EditorAnimation* animation;
    int selected_bone_count;
    bool clear_selection_on_up;
    void (*state_update)();
    void (*state_draw)();
    Vec2 command_world_position;
    Vec2 selection_center;
    Vec2 selection_center_world;
    SavedBone saved_bones[MAX_BONES];
};

static AnimationEditor g_animation_editor = {};
extern Shortcut g_animation_editor_shortcuts[];

static void UpdateSelectionCenter()
{
    EditorAnimation& en = *g_animation_editor.animation;
    if (!en.skeleton_asset)
        return;

    EditorSkeleton& es = *en.skeleton_asset->skeleton;

    Vec2 center = VEC2_ZERO;
    float center_count = 0.0f;
    for (int i=0; i<es.bone_count; i++)
    {
        EditorBone& eb = es.bones[i];
        if (!eb.selected)
            continue;
        center += en.bone_transforms[i] * VEC2_ZERO;
        center_count += 1.0f;
    }

    g_animation_editor.selection_center =
        center_count < F32_EPSILON
            ? center
            : center / center_count;
    g_animation_editor.selection_center_world = g_animation_editor.selection_center + g_animation_editor.asset->position;
}

static void SaveState()
{
    EditorAnimation& en = *g_animation_editor.animation;
    if (!en.skeleton_asset)
        return;

    EditorSkeleton& es = *en.skeleton_asset->skeleton;

    for (int i=1; i<es.bone_count; i++)
    {
        EditorBone& eb = es.bones[i];
        SavedBone& sb = g_animation_editor.saved_bones[i];
        sb.world_to_local = es.bones[eb.parent_index].world_to_local;
        sb.world_position = eb.local_to_world * VEC2_ZERO;
        sb.transform = en.bones[i].frames[en.current_frame];
    }

    UpdateSelectionCenter();
}

static void SetState(AnimationEditorState state, void (*state_update)(), void (*state_draw)())
{
    g_animation_editor.state = state;
    g_animation_editor.state_update = state_update;
    g_animation_editor.state_draw = state_draw;
    g_animation_editor.command_world_position = g_asset_editor.mouse_world_position;

    SetCursor(SYSTEM_CURSOR_DEFAULT);
}

static void ClearSelection()
{
    EditorAnimation& en = *g_animation_editor.animation;
    if (!en.skeleton_asset)
        return;

    EditorSkeleton& es = *en.skeleton_asset->skeleton;
    for (int i=0; i<es.bone_count; i++)
        es.bones[i].selected = false;

    g_animation_editor.selected_bone_count = 0;
}

static int HitTestBone(const EditorAnimation& en, const Vec2& world_pos)
{
    const float size = g_asset_editor.select_size;
    for (int i=0; i<en.bone_count; i++)
    {
        Vec2 bone_position = en.bone_transforms[i] * VEC2_ZERO;
        if (Length(bone_position - world_pos) < size)
            return i;
    }

    return -1;
}

static void SelectBone(int bone_index)
{
    EditorAnimation& en = *g_animation_editor.animation;
    if (!en.skeleton_asset)
        return;

    EditorSkeleton& es = *en.skeleton_asset->skeleton;

    ClearSelection();

    es.bones[bone_index].selected = true;
    g_animation_editor.selected_bone_count++;
}

static bool SelectBone()
{
    EditorAnimation& en = *g_animation_editor.animation;
    if (!en.skeleton_asset)
        return false;

    int bone_index = HitTestBone(
        en,
        ScreenToWorld(g_asset_editor.camera, GetMousePosition()) - g_animation_editor.asset->position);

    if (bone_index == -1)
        return false;

    SelectBone(bone_index);
    return true;
}

static void UpdateRotateState()
{
    EditorAnimation& en = *g_animation_editor.animation;
    if (!en.skeleton_asset)
        return;

    Vec2 dir_start = Normalize(g_animation_editor.command_world_position - g_animation_editor.selection_center_world);
    Vec2 dir_current = Normalize(g_asset_editor.mouse_world_position - g_animation_editor.selection_center_world);
    float angle = SignedAngleDelta(dir_start, dir_current);
    if (fabsf(angle) < F32_EPSILON)
        return;

    for (int i=0; i<en.bone_count; i++)
    {
        EditorBone& eb = en.skeleton_asset->skeleton->bones[i];
        if (!eb.selected)
            continue;

        SavedBone& sb = g_animation_editor.saved_bones[i];
        en.bones[i].frames[en.current_frame].rotation = sb.transform.rotation - angle;
    }

    UpdateTransforms(en, en.current_frame);
}

static void UpdateMoveState()
{
    Vec2 world_delta = g_asset_editor.mouse_world_position - g_animation_editor.command_world_position;

    EditorAnimation& en = *g_animation_editor.asset->anim;
    for (int i=0; i<en.bone_count; i++)
    {
        EditorAnimationBone& eab = en.bones[i];
        EditorBone& eb = en.skeleton_asset->skeleton->bones[i];
        if (!eb.selected)
            continue;

        SavedBone& sb = g_animation_editor.saved_bones[i];
        eab.frames[en.current_frame].position = sb.transform.position + world_delta;
    }

    UpdateTransforms(en, en.current_frame);
}

static void UpdatePlayState()
{
    Animation* animation = g_animation_editor.animation->animation;
    if (!animation)
        animation = g_animation_editor.animation->animation = ToAnimation(ALLOCATOR_DEFAULT, *g_animation_editor.animation, g_animation_editor.asset->name);

    if (!animation)
        return;

    // todo: how do we animate?
}

static void UpdateDefaultState()
{
    EditorAnimation& en = *g_animation_editor.animation;
    if (!en.skeleton_asset)
        return;

    EditorSkeleton& es = *en.skeleton_asset->skeleton;

    // If a drag has started then switch to box select
    if (g_asset_editor.drag)
    {
        //BeginBoxSelect(HandleBoxSelect);
        return;
    }

    // Select
    if (!g_asset_editor.drag && WasButtonReleased(g_asset_editor.input, MOUSE_LEFT))
    {
        g_animation_editor.clear_selection_on_up = false;

        if (SelectBone())
            return;

        g_animation_editor.clear_selection_on_up = true;
    }

    if (WasButtonReleased(g_asset_editor.input, MOUSE_LEFT) && g_animation_editor.clear_selection_on_up)
    {
        ClearSelection();
    }
}

void UpdateAnimationEditor()
{
    CheckShortcuts(g_animation_editor_shortcuts);
    UpdateBounds(*g_animation_editor.animation);

    if (g_animation_editor.state_update)
        g_animation_editor.state_update();

    if (g_animation_editor.state == ANIMATION_EDITOR_STATE_DEFAULT)
        UpdateDefaultState();

    // Commit the tool
    if (WasButtonPressed(g_asset_editor.input, MOUSE_LEFT) || WasButtonPressed(g_asset_editor.input, KEY_ENTER))
    {
        g_animation_editor.asset->modified = true;
        SetState(ANIMATION_EDITOR_STATE_DEFAULT, nullptr, nullptr);
    }
    // Cancel the tool
    else if (WasButtonPressed(g_asset_editor.input, KEY_ESCAPE) || WasButtonPressed(g_asset_editor.input, MOUSE_RIGHT))
    {
        CancelUndo();
        SetState(ANIMATION_EDITOR_STATE_DEFAULT, nullptr, nullptr);
    }
}

static void DrawSkeleton()
{
    EditorAsset& ea = *g_animation_editor.asset;
    EditorAnimation& en = *g_animation_editor.animation;
    if (!en.skeleton_asset)
        return;

    EditorSkeleton& es = *en.skeleton_asset->skeleton;

    for (int i=0; i<es.bone_count; i++)
    {
        const EditorBone& bone = es.bones[i];
        BindColor(bone.selected ? COLOR_SELECTED : COLOR_BLACK);
        DrawVertex(en.bone_transforms[i] * VEC2_ZERO + ea.position, BONE_ORIGIN_SIZE);
    }
}

static void DrawRotateState()
{
    BindColor(SetAlpha(COLOR_CENTER, 0.75f));
    DrawVertex(g_animation_editor.selection_center_world, CENTER_SIZE * 0.75f);
    BindColor(COLOR_CENTER);
    DrawLine(g_asset_editor.mouse_world_position, g_animation_editor.selection_center_world);
    BindColor(COLOR_ORIGIN);
    DrawVertex(g_asset_editor.mouse_world_position, CENTER_SIZE);
}

void DrawAnimationEditor()
{
    DrawSkeleton();

    if (g_animation_editor.state_draw)
        g_animation_editor.state_draw();

    EditorAsset& ea = *g_animation_editor.asset;
    EditorAnimation& en = *g_animation_editor.animation;

    Vec2 h1 =
        ScreenToWorld(g_asset_editor.camera, {g_asset_editor.dpi * FRAME_LINE_SIZE, 0}) -
        ScreenToWorld(g_asset_editor.camera, VEC2_ZERO);

    Vec2 pos = ea.position + Vec2 { 0, en.bounds.min.y + FRAME_LINE_OFFSET };
    Vec2 left = Vec2{h1.x * (en.frame_count - 1) * 0.5f, 0};

    BindColor(COLOR_BLACK);
    DrawLine(pos - left, pos + left);

    for (int i=0; i<en.frame_count; i++)
    {
        DrawVertex({pos.x - left.x + h1.x * i, pos.y}, FRAME_SIZE);
    }

    BindColor(COLOR_ORIGIN);
    DrawVertex({pos.x - left.x + h1.x * en.current_frame, pos.y}, FRAME_SELECTED_SIZE);
}

static void HandlePrevFrameCommand()
{
    EditorAnimation& en = *g_animation_editor.animation;
    en.current_frame = Max(0, en.current_frame - 1);
}

static void HandleNextFrameCommand()
{
    EditorAnimation& en = *g_animation_editor.animation;
    en.current_frame = Min(g_animation_editor.animation->frame_count - 1, en.current_frame + 1);
}

static void HandleMoveCommand()
{
    if (g_animation_editor.state != ANIMATION_EDITOR_STATE_DEFAULT)
        return;

    if (g_animation_editor.selected_bone_count <= 0)
        return;

    RecordUndo(*g_animation_editor.asset);
    SaveState();
    SetState(ANIMATION_EDITOR_STATE_MOVE, UpdateMoveState, nullptr);
    SetCursor(SYSTEM_CURSOR_MOVE);
}

static void HandleRotateCommand()
{
    if (g_animation_editor.state != ANIMATION_EDITOR_STATE_DEFAULT)
        return;

    if (g_animation_editor.selected_bone_count <= 0)
        return;

    RecordUndo(*g_animation_editor.asset);
    SaveState();
    SetState(ANIMATION_EDITOR_STATE_ROTATE, UpdateRotateState, DrawRotateState);
    //SetCursor(SYSTEM_CURSOR_MOVE);
}

static void HandlePlayCommand()
{
    if (g_animation_editor.state != ANIMATION_EDITOR_STATE_DEFAULT)
        return;

    SetState(ANIMATION_EDITOR_STATE_PLAY, UpdatePlayState, nullptr);
}

static Shortcut g_animation_editor_shortcuts[] = {
    { KEY_G, false, false, false, HandleMoveCommand },
    { KEY_R, false, false, false, HandleRotateCommand },
    { KEY_A, false, false, false, HandlePrevFrameCommand },
    { KEY_D, false, false, false, HandleNextFrameCommand },
    { KEY_SPACE, false, false, false, HandlePlayCommand },
    { INPUT_CODE_NONE }
};

void InitAnimationEditor(EditorAsset& ea)
{
    g_animation_editor.state = ANIMATION_EDITOR_STATE_DEFAULT;
    g_animation_editor.asset = &ea;
    g_animation_editor.animation = ea.anim;

    EnableShortcuts(g_animation_editor_shortcuts);
}

