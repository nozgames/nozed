//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include <view.h>

constexpr float FRAME_LINE_SIZE = 0.5f;
constexpr float FRAME_LINE_OFFSET = -0.2f;
constexpr float FRAME_SIZE = 0.16f;
constexpr float FRAME_SELECTED_SIZE = 0.32f;
constexpr float FRAME_TIME_SIZE = 0.32f;

constexpr float CENTER_SIZE = 0.2f;
constexpr float ORIGIN_SIZE = 0.1f;
constexpr float ORIGIN_BORDER_SIZE = 0.12f;
constexpr float ROTATE_TOOL_WIDTH = 0.02f;

constexpr float BONE_ORIGIN_SIZE = 0.16f;

enum AnimationViewState
{
    ANIMATION_VIEW_STATE_DEFAULT,
    ANIMATION_VIEW_STATE_MOVE,
    ANIMATION_VIEW_STATE_ROTATE,
    ANIMATION_VIEW_STATE_PLAY,
};

struct SavedBone
{
    Mat3 world_to_local;
    Vec2 world_position;
    BoneTransform transform;
};

struct AnimationView
{
    AnimationViewState state;
    EditorAsset* asset;
    EditorAnimation* animation;
    int selected_bone_count;
    bool clear_selection_on_up;
    bool ignore_up;
    void (*state_update)();
    void (*state_draw)();
    Vec2 command_world_position;
    Vec2 selection_center;
    Vec2 selection_center_world;
    SavedBone saved_bones[MAX_BONES];
    Animator animator;
};

static AnimationView g_animation_editor = {};
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

static void SetState(AnimationViewState state, void (*state_update)(), void (*state_draw)())
{
    g_animation_editor.state = state;
    g_animation_editor.state_update = state_update;
    g_animation_editor.state_draw = state_draw;
    g_animation_editor.command_world_position = g_view.mouse_world_position;

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

static void AddSelection(int bone_index)
{
    EditorAnimation& en = *g_animation_editor.animation;
    if (!en.skeleton_asset)
        return;

    EditorSkeleton& es = *en.skeleton_asset->skeleton;
    EditorBone& eb = es.bones[bone_index];
    if (eb.selected)
        return;

    eb.selected = true;
    g_animation_editor.selected_bone_count++;

}

static int HitTestBone(const EditorAnimation& en, const Vec2& world_pos)
{
    const float size = g_view.select_size;
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
        ScreenToWorld(g_view.camera, GetMousePosition()) - g_animation_editor.asset->position);

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
    Vec2 dir_current = Normalize(g_view.mouse_world_position - g_animation_editor.selection_center_world);
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
    Vec2 world_delta = g_view.mouse_world_position - g_animation_editor.command_world_position;

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
    if (g_view.drag)
    {
        //BeginBoxSelect(HandleBoxSelect);
        return;
    }

    // Select
    if (!g_animation_editor.ignore_up && !g_view.drag && WasButtonReleased(g_view.input, MOUSE_LEFT))
    {
        g_animation_editor.clear_selection_on_up = false;

        if (SelectBone())
            return;

        g_animation_editor.clear_selection_on_up = true;
    }

    g_animation_editor.ignore_up &= !WasButtonReleased(g_view.input, MOUSE_LEFT);

    if (WasButtonReleased(g_view.input, MOUSE_LEFT) && g_animation_editor.clear_selection_on_up)
    {
        ClearSelection();
    }
}

void UpdateAnimationEditor()
{
    CheckShortcuts(g_animation_editor_shortcuts);
    UpdateBounds(*g_animation_editor.animation);

    // Commit the tool
    if (g_animation_editor.state == ANIMATION_VIEW_STATE_MOVE ||
        g_animation_editor.state == ANIMATION_VIEW_STATE_ROTATE)
    {
        if (WasButtonPressed(g_view.input, MOUSE_LEFT) || WasButtonPressed(g_view.input, KEY_ENTER))
        {
            MarkModified(*g_animation_editor.asset);
            g_animation_editor.ignore_up = true;
            SetState(ANIMATION_VIEW_STATE_DEFAULT, nullptr, nullptr);
            return;
        }

        // Cancel the tool
        if (WasButtonPressed(g_view.input, KEY_ESCAPE) || WasButtonPressed(g_view.input, MOUSE_RIGHT))
        {
            CancelUndo();
            SetState(ANIMATION_VIEW_STATE_DEFAULT, nullptr, nullptr);
            return;
        }
    }

    if (g_animation_editor.state_update)
        g_animation_editor.state_update();

    if (g_animation_editor.state == ANIMATION_VIEW_STATE_DEFAULT)
        UpdateDefaultState();
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

    if (IsPlaying(g_animation_editor.animator))
    {
        Update(g_animation_editor.animator);

        BindMaterial(g_view.vertex_material);
        BindColor(COLOR_RED);
        for (int i=1; i<en.bone_count; i++)
        {
            Vec2 b1 = g_animation_editor.animator.bones[es.bones[i].parent_index] * VEC2_ZERO;
            Vec2 b2 = g_animation_editor.animator.bones[i] * VEC2_ZERO;
            DrawBone(b2 + ea.position, b1 + ea.position);
        }
    }
}

static void DrawRotateState()
{
    BindColor(SetAlpha(COLOR_CENTER, 0.75f));
    DrawVertex(g_animation_editor.selection_center_world, CENTER_SIZE * 0.75f);
    BindColor(COLOR_CENTER);
    DrawDashedLine(g_view.mouse_world_position, g_animation_editor.selection_center_world);
    BindColor(COLOR_ORIGIN);
    DrawVertex(g_view.mouse_world_position, CENTER_SIZE);
}

static void DrawTimeline()
{
    EditorAsset& ea = *g_animation_editor.asset;
    EditorAnimation& en = *g_animation_editor.animation;

    Vec2 h1 =
        ScreenToWorld(g_view.camera, {g_view.dpi * FRAME_LINE_SIZE, 0}) -
        ScreenToWorld(g_view.camera, VEC2_ZERO);

    Vec2 pos = ea.position + Vec2 { 0, en.bounds.min.y + FRAME_LINE_OFFSET };
    Vec2 left = Vec2{h1.x * (en.frame_count - 1) * 0.5f, 0};
    Vec2 right = -left;

    BindColor(COLOR_BLACK);
    DrawLine(pos - left, pos + left);

    for (int i=0; i<en.frame_count; i++)
        DrawVertex({pos.x - left.x + h1.x * i, pos.y}, FRAME_SIZE);

    int current_frame = en.current_frame;
    if (IsPlaying(g_animation_editor.animator))
    {
        current_frame = GetFrame(g_animation_editor.animator);
        BindColor({0.02f, 0.02f, 0.02f, 1.0f});
        DrawLine(pos + left, pos + left + h1);
        DrawVertex(pos + left + h1, FRAME_SIZE * 0.9f);
    }

    BindColor(COLOR_ORIGIN);
    DrawVertex({pos.x - left.x + h1.x * current_frame, pos.y}, FRAME_SELECTED_SIZE);

    if (IsPlaying(g_animation_editor.animator))
    {
        Vec2 s2 =
                ScreenToWorld(g_view.camera, {0, g_view.dpi * FRAME_TIME_SIZE}) -
                ScreenToWorld(g_view.camera, VEC2_ZERO);


        BindColor(COLOR_WHITE);
        float time = GetNormalizedTime(g_animation_editor.animator);
        Vec2 tpos = pos + Mix(right, left, time);
        DrawLine(tpos - s2, tpos + s2);
    }
}

void DrawAnimationEditor()
{
    DrawSkeleton();
    DrawTimeline();

    if (g_animation_editor.state_draw)
        g_animation_editor.state_draw();
}

static void HandlePrevFrameCommand()
{
    EditorAnimation& en = *g_animation_editor.animation;
    en.current_frame = (en.current_frame - 1 + g_animation_editor.animation->frame_count) % g_animation_editor.animation->frame_count;
}

static void HandleNextFrameCommand()
{
    EditorAnimation& en = *g_animation_editor.animation;
    en.current_frame = (en.current_frame + 1) % g_animation_editor.animation->frame_count;
}

static void HandleMoveCommand()
{
    if (g_animation_editor.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    if (g_animation_editor.selected_bone_count <= 0)
        return;

    RecordUndo(*g_animation_editor.asset);
    SaveState();
    SetState(ANIMATION_VIEW_STATE_MOVE, UpdateMoveState, nullptr);
    SetCursor(SYSTEM_CURSOR_MOVE);
}

static void HandleRotate()
{
    if (g_animation_editor.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    if (g_animation_editor.selected_bone_count <= 0)
        return;

    RecordUndo(*g_animation_editor.asset);
    SaveState();
    SetState(ANIMATION_VIEW_STATE_ROTATE, UpdateRotateState, DrawRotateState);
    //SetCursor(SYSTEM_CURSOR_MOVE);
}

static void HandleResetRotate()
{
    if (g_animation_editor.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    RecordUndo(*g_animation_editor.asset);
    EditorAnimation& en = *g_animation_editor.animation;
    for (int i=0; i<en.bone_count; i++)
    {
        EditorBone& eb = en.skeleton_asset->skeleton->bones[i];
        if (!eb.selected)
            continue;

        en.bones[i].frames[en.current_frame].rotation = 0;
    }

    UpdateTransforms(en, en.current_frame);
}

static void HandlePlayCommand()
{
    if (g_animation_editor.state == ANIMATION_VIEW_STATE_PLAY)
    {
        Stop(g_animation_editor.animator);
        SetState(ANIMATION_VIEW_STATE_DEFAULT, nullptr, nullptr);
        return;
    }

    if (g_animation_editor.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    EditorAsset& ea = *g_animation_editor.asset;
    EditorAnimation& en = *g_animation_editor.animation;
    if (!en.skeleton_asset)
        return;

    EditorSkeleton& es = *en.skeleton_asset->skeleton;

    Init(
        g_animation_editor.animator,
        ToSkeleton(ALLOCATOR_DEFAULT, es, en.skeleton_asset->name));
    Play(g_animation_editor.animator, ToAnimation(ALLOCATOR_DEFAULT, en, ea.name), 0.1f, true);
    SetState(ANIMATION_VIEW_STATE_PLAY, UpdatePlayState, nullptr);
}

static void HandleResetMoveCommand()
{
    if (g_animation_editor.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    RecordUndo(*g_animation_editor.asset);
    EditorAnimation& en = *g_animation_editor.animation;
    for (int i=0; i<en.bone_count; i++)
    {
        EditorBone& eb = en.skeleton_asset->skeleton->bones[i];
        if (!eb.selected)
            continue;

        en.bones[i].frames[en.current_frame].position = VEC2_ZERO;
    }

    UpdateTransforms(en, en.current_frame);
}

static void HandleSelectAll()
{
    if (g_animation_editor.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    EditorAnimation& en = *g_animation_editor.animation;
    for (int i=0; i<en.bone_count; i++)
        AddSelection(i);
}

static Shortcut g_animation_editor_shortcuts[] = {
    { KEY_G, false, false, false, HandleMoveCommand },
    { KEY_G, true, false, false, HandleResetMoveCommand },
    { KEY_R, false, false, false, HandleRotate },
    { KEY_R, true, false, false, HandleResetRotate },
    { KEY_A, false, false, false, HandleSelectAll },
    { KEY_Q, false, false, false, HandlePrevFrameCommand },
    { KEY_E, false, false, false, HandleNextFrameCommand },
    { KEY_SPACE, false, false, false, HandlePlayCommand },
    { INPUT_CODE_NONE }
};

void InitAnimationEditor(EditorAsset& ea)
{
    g_animation_editor.state = ANIMATION_VIEW_STATE_DEFAULT;
    g_animation_editor.asset = &ea;
    g_animation_editor.animation = ea.anim;

    EnableShortcuts(g_animation_editor_shortcuts);
}

