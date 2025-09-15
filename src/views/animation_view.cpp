//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include <editor.h>

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

struct AnimationViewBone
{
    Transform transform;
    bool selected;
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
    AnimationViewBone bones[MAX_BONES];
    Animator animator;
};

static AnimationView g_animation_view = {};

static Shortcut* g_animation_editor_shortcuts;

static EditorAsset& GetEditorAsset() { return *g_animation_view.asset; }
static EditorAnimation& GetEditorAnimation() { return *g_animation_view.animation; }
static EditorSkeleton& GetEditorSkeleton() { return *GetEditorAnimation().skeleton_asset->skeleton; }
static bool IsBoneSelected(int bone_index) { return g_animation_view.bones[bone_index].selected; }
static void SetBoneSelected(int bone_index, bool selected)
{
    if (IsBoneSelected(bone_index) == selected)
        return;
    g_animation_view.bones[bone_index].selected = selected;
    g_animation_view.selected_bone_count += selected ? 1 : -1;
}

static void UpdateSelectionCenter()
{
    EditorAnimation& en = GetEditorAnimation();
    EditorSkeleton& es = GetEditorSkeleton();

    Vec2 center = VEC2_ZERO;
    float center_count = 0.0f;
    for (int bone_index=0; bone_index<es.bone_count; bone_index++)
    {
        if (!IsBoneSelected(bone_index))
            continue;
        center += TransformPoint(GetFrameTransform(en, bone_index, en.current_frame));
        center_count += 1.0f;
    }

    g_animation_view.selection_center =
        center_count < F32_EPSILON
            ? center
            : center / center_count;
    g_animation_view.selection_center_world = g_animation_view.selection_center + g_animation_view.asset->position;
}

static void SaveState()
{
    EditorAnimation& en = GetEditorAnimation();
    EditorSkeleton& es = GetEditorSkeleton();
    for (int bone_index=1; bone_index<es.bone_count; bone_index++)
        g_animation_view.bones[bone_index].transform = GetFrameTransform(en, bone_index, en.current_frame);

    UpdateSelectionCenter();
}

static void SetState(AnimationViewState state, void (*state_update)(), void (*state_draw)())
{
    g_animation_view.state = state;
    g_animation_view.state_update = state_update;
    g_animation_view.state_draw = state_draw;
    g_animation_view.command_world_position = g_view.mouse_world_position;

    SetCursor(SYSTEM_CURSOR_DEFAULT);
}

static void ClearSelection()
{
    EditorSkeleton& es = GetEditorSkeleton();
    for (int bone_index=0; bone_index<es.bone_count; bone_index++)
        SetBoneSelected(bone_index, false);
}

static void AddSelection(int bone_index)
{
    SetBoneSelected(bone_index, true);
}

static void SelectBone(int bone_index)
{
    ClearSelection();
    SetBoneSelected(bone_index, true);
}

static bool SelectBone()
{
    EditorAnimation& en = GetEditorAnimation();
    int bone_index = HitTestBone(
        en,
        g_view.mouse_world_position - g_animation_view.asset->position);

    if (bone_index == -1)
        return false;

    SelectBone(bone_index);
    return true;
}

static void UpdateRotateState()
{
    EditorAnimation& en = GetEditorAnimation();
    EditorSkeleton& es = GetEditorSkeleton();

    Vec2 dir_start = Normalize(g_animation_view.command_world_position - g_animation_view.selection_center_world);
    Vec2 dir_current = Normalize(g_view.mouse_world_position - g_animation_view.selection_center_world);
    float angle = SignedAngleDelta(dir_start, dir_current);
    if (fabsf(angle) < F32_EPSILON)
        return;

    for (int bone_index=0; bone_index<es.bone_count; bone_index++)
    {
        if (!IsBoneSelected(bone_index))
            continue;

        AnimationViewBone& sb = g_animation_view.bones[bone_index];
        SetRotation(GetFrameTransform(en, bone_index, en.current_frame), sb.transform.rotation + angle);
    }
}

static void UpdateMoveState()
{
    EditorAnimation& en = GetEditorAnimation();
    EditorSkeleton& es = GetEditorSkeleton();

    Vec2 world_delta = g_view.mouse_world_position - g_animation_view.command_world_position;
    for (int bone_index=0; bone_index<es.bone_count; bone_index++)
    {
        if (!IsBoneSelected(bone_index))
            continue;

        AnimationViewBone& sb = g_animation_view.bones[bone_index];
        SetPosition(GetFrameTransform(en, bone_index, en.current_frame), sb.transform.position + world_delta);
    }

    UpdateTransforms(en);
}

static void UpdateAssetNames()
{
    if (g_animation_view.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    if (!IsAltDown(g_view.input))
        return;

    EditorAsset& ea = GetEditorAsset();
    EditorAnimation& en = GetEditorAnimation();
    EditorSkeleton& es = GetEditorSkeleton();
    for (u16 bone_index=0; bone_index<es.bone_count; bone_index++)
    {
        BeginWorldCanvas(g_view.camera, ea.position + TransformPoint(GetFrameTransform(en, bone_index, en.current_frame)), Vec2{6, 0});
        SetStyleSheet(g_assets.ui.view);
        BeginElement(g_names.asset_name_container);
        Label(es.bones[bone_index].name->value, g_names.asset_name);
        EndElement();
        EndCanvas();
    }
}

static void UpdatePlayState()
{
    Animation* animation = g_animation_view.animation->animation;
    if (!animation)
        animation = g_animation_view.animation->animation = ToAnimation(ALLOCATOR_DEFAULT, *g_animation_view.animation, g_animation_view.asset->name);

    if (!animation)
        return;

    // todo: how do we animate?
}

static void UpdateDefaultState()
{
    // If a drag has started then switch to box select
    if (g_view.drag)
    {
        //BeginBoxSelect(HandleBoxSelect);
        return;
    }

    if (!g_animation_view.ignore_up && !g_view.drag && WasButtonReleased(g_view.input, MOUSE_LEFT))
    {
        g_animation_view.clear_selection_on_up = false;

        if (SelectBone())
            return;

        g_animation_view.clear_selection_on_up = true;
    }

    g_animation_view.ignore_up &= !WasButtonReleased(g_view.input, MOUSE_LEFT);

    if (WasButtonReleased(g_view.input, MOUSE_LEFT) && g_animation_view.clear_selection_on_up)
    {
        ClearSelection();
    }
}

void UpdateAnimationEditor()
{
    CheckShortcuts(g_animation_editor_shortcuts);
    UpdateBounds(*g_animation_view.animation);
    UpdateAssetNames();

    // Commit the tool
    if (g_animation_view.state == ANIMATION_VIEW_STATE_MOVE ||
        g_animation_view.state == ANIMATION_VIEW_STATE_ROTATE)
    {
        if (WasButtonPressed(g_view.input, MOUSE_LEFT) || WasButtonPressed(g_view.input, KEY_ENTER))
        {
            MarkModified(*g_animation_view.asset);
            g_animation_view.ignore_up = true;
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

    if (g_animation_view.state_update)
        g_animation_view.state_update();

    if (g_animation_view.state == ANIMATION_VIEW_STATE_DEFAULT)
        UpdateDefaultState();
}

static void DrawSkeleton()
{
    EditorAsset& ea = GetEditorAsset();
    EditorAnimation& en = GetEditorAnimation();
    EditorSkeleton& es = GetEditorSkeleton();

    BindColor(COLOR_SELECTED);
    for (int bone_index=0; bone_index<es.bone_count; bone_index++)
    {
        if (!IsBoneSelected(bone_index))
            continue;

        DrawVertex(TransformPoint(en.animator.bones[bone_index]) + ea.position, BONE_ORIGIN_SIZE);
    }

    if (IsPlaying(g_animation_view.animator))
    {
        Update(g_animation_view.animator);

        BindMaterial(g_view.vertex_material);
        BindColor(COLOR_RED);
        for (int i=1; i<es.bone_count; i++)
        {
            Vec2 b1 = g_animation_view.animator.bones[es.bones[i].parent_index] * VEC2_ZERO;
            Vec2 b2 = g_animation_view.animator.bones[i] * VEC2_ZERO;
            DrawBone(b2 + ea.position, b1 + ea.position);
        }
    }
}

static void DrawRotateState()
{
    BindColor(SetAlpha(COLOR_CENTER, 0.75f));
    DrawVertex(g_animation_view.selection_center_world, CENTER_SIZE * 0.75f);
    BindColor(COLOR_CENTER);
    DrawDashedLine(g_view.mouse_world_position, g_animation_view.selection_center_world);
    BindColor(COLOR_ORIGIN);
    DrawVertex(g_view.mouse_world_position, CENTER_SIZE);
}

static void DrawTimeline()
{
    EditorAsset& ea = GetEditorAsset();
    EditorAnimation& en = GetEditorAnimation();

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
    if (IsPlaying(g_animation_view.animator))
    {
        current_frame = GetFrame(g_animation_view.animator);
        BindColor({0.02f, 0.02f, 0.02f, 1.0f});
        DrawLine(pos + left, pos + left + h1);
        DrawVertex(pos + left + h1, FRAME_SIZE * 0.9f);
    }

    BindColor(COLOR_ORIGIN);
    DrawVertex({pos.x - left.x + h1.x * current_frame, pos.y}, FRAME_SELECTED_SIZE);

    if (IsPlaying(g_animation_view.animator))
    {
        Vec2 s2 =
                ScreenToWorld(g_view.camera, {0, g_view.dpi * FRAME_TIME_SIZE}) -
                ScreenToWorld(g_view.camera, VEC2_ZERO);


        BindColor(COLOR_WHITE);
        float time = GetNormalizedTime(g_animation_view.animator);
        Vec2 tpos = pos + Mix(right, left, time);
        DrawLine(tpos - s2, tpos + s2);
    }
}

void DrawAnimationEditor()
{
    DrawSkeleton();
    DrawTimeline();

    if (g_animation_view.state_draw)
        g_animation_view.state_draw();
}

static void HandlePrevFrameCommand()
{
    EditorAnimation& en = *g_animation_view.animation;
    en.current_frame = (en.current_frame - 1 + g_animation_view.animation->frame_count) % g_animation_view.animation->frame_count;
}

static void HandleNextFrameCommand()
{
    EditorAnimation& en = *g_animation_view.animation;
    en.current_frame = (en.current_frame + 1) % g_animation_view.animation->frame_count;
}

static void HandleMoveCommand()
{
    if (g_animation_view.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    if (g_animation_view.selected_bone_count <= 0)
        return;

    RecordUndo(*g_animation_view.asset);
    SaveState();
    SetState(ANIMATION_VIEW_STATE_MOVE, UpdateMoveState, nullptr);
    SetCursor(SYSTEM_CURSOR_MOVE);
}

static void HandleRotate()
{
    if (g_animation_view.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    if (g_animation_view.selected_bone_count <= 0)
        return;

    RecordUndo(*g_animation_view.asset);
    SaveState();
    SetState(ANIMATION_VIEW_STATE_ROTATE, UpdateRotateState, DrawRotateState);
    //SetCursor(SYSTEM_CURSOR_MOVE);
}

static void HandleResetRotate()
{
    if (g_animation_view.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    RecordUndo(*g_animation_view.asset);
    EditorAnimation& en = GetEditorAnimation();
    EditorSkeleton& es = GetEditorSkeleton();
    for (int bone_index=0; bone_index<es.bone_count; bone_index++)
    {
        if (!IsBoneSelected(bone_index))
            continue;

        SetRotation(GetFrameTransform(en, bone_index, en.current_frame), 0);
    }
}

static void HandlePlayCommand()
{
    if (g_animation_view.state == ANIMATION_VIEW_STATE_PLAY)
    {
        Stop(g_animation_view.animator);
        SetState(ANIMATION_VIEW_STATE_DEFAULT, nullptr, nullptr);
        return;
    }

    if (g_animation_view.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    EditorAsset& ea = GetEditorAsset();
    EditorAnimation& en = GetEditorAnimation();
    EditorSkeleton& es = GetEditorSkeleton();

    Init(
        g_animation_view.animator,
        ToSkeleton(ALLOCATOR_DEFAULT, es, en.skeleton_asset->name));
    Play(g_animation_view.animator, ToAnimation(ALLOCATOR_DEFAULT, en, ea.name), 0.1f, true);
    SetState(ANIMATION_VIEW_STATE_PLAY, UpdatePlayState, nullptr);
}

static void HandleResetMoveCommand()
{
    if (g_animation_view.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    RecordUndo(*g_animation_view.asset);

    EditorAnimation& en = GetEditorAnimation();
    EditorSkeleton& es = GetEditorSkeleton();
    for (int bone_index=0; bone_index<es.bone_count; bone_index++)
    {
        if (!IsBoneSelected(bone_index))
            continue;

        SetPosition(GetFrameTransform(en, bone_index, en.current_frame), VEC2_ZERO);
    }
}

static void HandleSelectAll()
{
    if (g_animation_view.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    EditorSkeleton& es = GetEditorSkeleton();
    for (int i=0; i<es.bone_count; i++)
        AddSelection(i);
}

static void HandleInsertBeforeFrame()
{
    EditorAnimation& en = GetEditorAnimation();
    en.current_frame = InsertFrame(en, en.current_frame);
}

static void HandleInsertAfterFrame()
{
    EditorAnimation& en = GetEditorAnimation();
    en.current_frame = InsertFrame(en, en.current_frame + 1);
}

static void HandleDeleteFrame()
{
    EditorAnimation& en = GetEditorAnimation();
    en.current_frame = DeleteFrame(en, en.current_frame);
}

void InitAnimationEditor(EditorAsset& ea)
{
    g_animation_view.state = ANIMATION_VIEW_STATE_DEFAULT;
    g_animation_view.asset = &ea;
    g_animation_view.animation = ea.anim;

    if (g_animation_editor_shortcuts == nullptr)
    {
        static Shortcut shortcuts[] = {
            { KEY_G, false, false, false, HandleMoveCommand },
            { KEY_G, true, false, false, HandleResetMoveCommand },
            { KEY_R, false, false, false, HandleRotate },
            { KEY_R, true, false, false, HandleResetRotate },
            { KEY_A, false, false, false, HandleSelectAll },
            { KEY_Q, false, false, false, HandlePrevFrameCommand },
            { KEY_E, false, false, false, HandleNextFrameCommand },
            { KEY_SPACE, false, false, false, HandlePlayCommand },
            { KEY_I, false, false, false, HandleInsertBeforeFrame },
            { KEY_O, false, false, false, HandleInsertAfterFrame },
            { KEY_X, false, false, false, HandleDeleteFrame },
            { INPUT_CODE_NONE }
        };

        g_animation_editor_shortcuts = shortcuts;
        EnableShortcuts(g_animation_editor_shortcuts);
    }
}

