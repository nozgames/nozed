//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "editor_assets.h"

#include <editor.h>

constexpr float FRAME_LINE_SIZE = 0.5f;
constexpr float FRAME_LINE_OFFSET = -0.2f;
constexpr float FRAME_SIZE_X = 0.3f;
constexpr float FRAME_SIZE_Y = 0.8f;
constexpr float FRAME_BORDER_SIZE = 0.025f;
constexpr float FRAME_SELECTED_SIZE = 0.32f;
constexpr float FRAME_TIME_SIZE = 0.32f;
constexpr float FRAME_DOT_SIZE = 0.1f;
constexpr Color FRAME_COLOR = Color32ToColor(100, 100, 100, 255);
constexpr Color FRAME_SELECTED_COLOR = COLOR_VERTEX_SELECTED;

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
};

struct AnimationView
{
    AnimationViewState state;
    bool clear_selection_on_up;
    bool ignore_up;
    void (*state_update)();
    void (*state_draw)();
    Vec2 command_world_position;
    Vec2 selection_center;
    Vec2 selection_center_world;
    AnimationViewBone bones[MAX_BONES];
    bool onion_skin;
};

static AnimationView g_animation_view = {};

static Shortcut* g_animation_editor_shortcuts;

static EditorAnimation* GetEditingAnimation()
{
    EditorAsset* ea = GetEditingAsset();
    assert(ea);
    assert(ea->type == EDITOR_ASSET_TYPE_ANIMATION);
    return (EditorAnimation*)ea;
}

static EditorSkeleton* GetEditingSkeleton() { return GetEditingAnimation()->skeleton; }
static bool IsBoneSelected(int bone_index) { return GetEditingAnimation()->bones[bone_index].selected; }
static void SetBoneSelected(int bone_index, bool selected)
{
    if (IsBoneSelected(bone_index) == selected)
        return;

    EditorAnimation* en = GetEditingAnimation();
    en->bones[bone_index].selected = selected;
    en->selected_bone_count += selected ? 1 : -1;
}

static void UpdateSelectionCenter()
{
    EditorAsset& ea = *GetEditingAsset();
    EditorAnimation* en = GetEditingAnimation();
    EditorSkeleton* es = GetEditingSkeleton();

    Vec2 center = VEC2_ZERO;
    float center_count = 0.0f;
    for (int bone_index=0; bone_index<es->bone_count; bone_index++)
    {
        if (!IsBoneSelected(bone_index))
            continue;
        center += TransformPoint(en->animator.bones[bone_index]);
        center_count += 1.0f;
    }

    g_animation_view.selection_center =
        center_count < F32_EPSILON
            ? center
            : center / center_count;
    g_animation_view.selection_center_world = g_animation_view.selection_center + ea.position;
}

static void SaveState()
{
    EditorAnimation* en = GetEditingAnimation();
    EditorSkeleton* es = GetEditingSkeleton();
    for (int bone_index=1; bone_index<es->bone_count; bone_index++)
        g_animation_view.bones[bone_index].transform = GetFrameTransform(en, bone_index, en->current_frame);

    UpdateSelectionCenter();
}

static void RevertToSavedState()
{
    EditorSkeleton* es = GetEditingSkeleton();
    EditorAnimation* en = GetEditingAnimation();
    for (int bone_index=1; bone_index<es->bone_count; bone_index++)
    {
        AnimationViewBone& vb = g_animation_view.bones[bone_index];
        GetFrameTransform(en, bone_index, en->current_frame) = vb.transform;
    }

    UpdateTransforms(en);
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
    EditorSkeleton* es = GetEditingSkeleton();
    for (int bone_index=0; bone_index<es->bone_count; bone_index++)
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
    EditorAsset& ea = *GetEditingAsset();
    EditorAnimation* en = GetEditingAnimation();
    int bone_index = HitTestBone(en, g_view.mouse_world_position - ea.position);
    if (bone_index == -1)
        return false;

    SelectBone(bone_index);
    return true;
}

static void UpdateRotateState()
{
    EditorAnimation* en = GetEditingAnimation();
    EditorSkeleton* es = GetEditingSkeleton();

    Vec2 dir_start = Normalize(g_animation_view.command_world_position - g_animation_view.selection_center_world);
    Vec2 dir_current = Normalize(g_view.mouse_world_position - g_animation_view.selection_center_world);
    float angle = SignedAngleDelta(dir_start, dir_current);
    if (fabsf(angle) < F32_EPSILON)
        return;

    for (int bone_index=0; bone_index<es->bone_count; bone_index++)
    {
        if (!IsBoneSelected(bone_index))
            continue;

        AnimationViewBone& sb = g_animation_view.bones[bone_index];
        SetRotation(GetFrameTransform(en, bone_index, en->current_frame), sb.transform.rotation + angle);
    }

    UpdateTransforms(en);
}

static void UpdateMoveState()
{
    EditorAnimation* en = GetEditingAnimation();
    EditorSkeleton* es = GetEditingSkeleton();

    Vec2 world_delta = g_view.mouse_world_position - g_animation_view.command_world_position;
    for (int bone_index=0; bone_index<es->bone_count; bone_index++)
    {
        if (!IsBoneSelected(bone_index))
            continue;

        AnimationViewBone& sb = g_animation_view.bones[bone_index];
        SetPosition(GetFrameTransform(en, bone_index, en->current_frame), sb.transform.position + world_delta);
    }

    UpdateTransforms(en);
}

static void UpdateAssetNames()
{
    if (g_animation_view.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    if (!IsAltDown(g_view.input))
        return;

    EditorAsset& ea = *GetEditingAsset();
    EditorAnimation* en = GetEditingAnimation();
    EditorSkeleton* es = GetEditingSkeleton();
    for (u16 bone_index=0; bone_index<es->bone_count; bone_index++)
    {
        Vec2 p =
            (TransformPoint(en->animator.bones[bone_index]) +
             TransformPoint(en->animator.bones[bone_index], {1,0})) * 0.5f;

        BeginWorldCanvas(g_view.camera, ea.position + p, Vec2{2, 2});
            BeginElement(STYLE_VIEW_ASSET_NAME_CONTAINER);
                Label(es->bones[bone_index].name->value, STYLE_VIEW_ASSET_NAME);
            EndElement();
        EndCanvas();
    }
}

static void UpdatePlayState()
{
    EditorAsset& ea = *GetEditingAsset();
    EditorAnimation* en = GetEditingAnimation();
    if (!en->animation)
        en->animation = ToAnimation(ALLOCATOR_DEFAULT, en, ea.name);

    if (!en->animation)
        return;

    Update(en->animator);
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
        ClearSelection();
}

void AnimationViewUpdate()
{
    EditorAnimation* ea = GetEditingAnimation();
    CheckShortcuts(g_animation_editor_shortcuts);
    UpdateBounds(ea);
    UpdateAssetNames();

    // Commit the tool
    if (g_animation_view.state == ANIMATION_VIEW_STATE_MOVE ||
        g_animation_view.state == ANIMATION_VIEW_STATE_ROTATE)
    {
        if (WasButtonPressed(g_view.input, MOUSE_LEFT) || WasButtonPressed(g_view.input, KEY_ENTER))
        {
            MarkModified();
            g_animation_view.ignore_up = true;
            SetState(ANIMATION_VIEW_STATE_DEFAULT, nullptr, nullptr);
            return;
        }

        // Cancel the tool
        if (WasButtonPressed(g_view.input, KEY_ESCAPE) || WasButtonPressed(g_view.input, MOUSE_RIGHT))
        {
            CancelUndo();
            RevertToSavedState();
            SetState(ANIMATION_VIEW_STATE_DEFAULT, nullptr, nullptr);
            return;
        }
    }

    if (g_animation_view.state_update)
        g_animation_view.state_update();

    if (g_animation_view.state == ANIMATION_VIEW_STATE_DEFAULT)
        UpdateDefaultState();
}

static void DrawOnionSkin()
{
    EditorAsset& ea = *GetEditingAsset();
    EditorSkeleton* es = GetEditingSkeleton();
    EditorAnimation* en = GetEditingAnimation();

    if (!g_animation_view.onion_skin || en->frame_count <= 1)
        return;

    int frame = en->current_frame;

    en->current_frame = (frame - 1 + en->frame_count) % en->frame_count;
    UpdateTransforms(en);

    BindMaterial(g_view.vertex_material);
    BindColor(SetAlpha(COLOR_RED, 0.25f));
    for (int bone_index=0; bone_index<es->bone_count; bone_index++)
    {
        DrawBone(
            en->animator.bones[bone_index] * Rotate(es->bones[bone_index].transform.rotation),
            es->bones[bone_index].parent_index < 0
                ? en->animator.bones[bone_index]
                : en->animator.bones[es->bones[bone_index].parent_index],
            ea.position);
    }

    en->current_frame = (frame + 1 + en->frame_count) % en->frame_count;
    UpdateTransforms(en);

    BindColor(SetAlpha(COLOR_GREEN, 0.25f));
    for (int bone_index=0; bone_index<es->bone_count; bone_index++)
    {
        DrawBone(
            en->animator.bones[bone_index] * Rotate(es->bones[bone_index].transform.rotation),
            es->bones[bone_index].parent_index < 0
                ? en->animator.bones[bone_index]
                : en->animator.bones[es->bones[bone_index].parent_index],
            ea.position);
    }

    en->current_frame = frame;
    UpdateTransforms(en);
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
    EditorAsset& ea = *GetEditingAsset();
    EditorAnimation* en = GetEditingAnimation();

    int real_frame_count = en->frame_count;
    for (int frame_index=0; frame_index<en->frame_count; frame_index++)
        real_frame_count += en->frames[frame_index].hold;

    Vec2 h1 =
        ScreenToWorld(g_view.camera, {g_view.dpi * FRAME_SIZE_X, g_view.dpi * -FRAME_SIZE_Y}) -
        ScreenToWorld(g_view.camera, VEC2_ZERO);
    float h2 =
        (ScreenToWorld(g_view.camera, {g_view.dpi * FRAME_BORDER_SIZE, 0}) -
         ScreenToWorld(g_view.camera, VEC2_ZERO)).x;

    Vec2 pos = ea.position + Vec2 { 0, en->bounds.min.y + FRAME_LINE_OFFSET };
    pos.x -= h1.x * real_frame_count * 0.5f;
    pos.x -= h2 * 0.5f;
    pos.y -= h2 * 0.5f;
    pos.y -= h1.y;
    int current_frame = en->current_frame;

    BindMaterial(g_view.vertex_material);
    int ii = 0;
    for (int i=0; i<en->frame_count; i++)
    {
        Rect frame_rect = Rect {
            pos.x + h1.x * ii,
            pos.y,
            h1.x + h2 + en->frames[i].hold * h1.x,
            h1.y + h2};
        BindColor(COLOR_BLACK);
        DrawRect(frame_rect);
        BindColor(i == current_frame ? FRAME_SELECTED_COLOR : FRAME_COLOR);
        DrawRect(Expand(frame_rect, -h2));
        BindColor(COLOR_BLACK);
        DrawVertex(Vec2{frame_rect.x + h2 + h1.x * 0.5f, frame_rect.y + frame_rect.height * 0.25f}, FRAME_DOT_SIZE);

        ii += (1 + en->frames[i].hold);
    }

    if (IsPlaying(en->animator))
    {
        current_frame = GetFrame(en->animator);
        BindColor({0.02f, 0.02f, 0.02f, 1.0f});
        //DrawLine(pos + left, pos + left + h1);
        //DrawVertex(pos + left + h1, FRAME_SIZE * 0.9f);
    }

    // BindColor(COLOR_ORIGIN);
    // DrawVertex({pos.x - left.x + h1.x * current_frame, pos.y}, FRAME_SELECTED_SIZE);
    //
    // if (IsPlaying(en->animator))
    // {
    //     Vec2 s2 =
    //             ScreenToWorld(g_view.camera, {0, g_view.dpi * FRAME_TIME_SIZE}) -
    //             ScreenToWorld(g_view.camera, VEC2_ZERO);
    //
    //
    //     BindColor(COLOR_WHITE);
    //     float time = GetNormalizedTime(en->animator);
    //     Vec2 tpos = pos + Mix(right, left, time);
    //     DrawLine(tpos - s2, tpos + s2);
    // }
}

void AnimationViewDraw()
{
    EditorAsset& ea = *GetEditingAsset();
    EditorSkeleton* es = GetEditingSkeleton();
    EditorAnimation* en = GetEditingAnimation();

    BindColor(COLOR_WHITE);
    for (int i=0; i<es->skinned_mesh_count; i++)
    {
        EditorMesh* skinned_mesh = es->skinned_meshes[i].mesh;
        if (!skinned_mesh || skinned_mesh->type != EDITOR_ASSET_TYPE_MESH)
            continue;

        DrawMesh(skinned_mesh, Translate(ea.position) * en->animator.bones[es->skinned_meshes[i].bone_index]);
    }

    DrawOnionSkin();

    BindMaterial(g_view.vertex_material);
    BindColor(COLOR_EDGE);
    for (int bone_index=0; bone_index<es->bone_count; bone_index++)
        DrawEditorAnimationBone(en, bone_index, ea.position);

    BindColor(COLOR_EDGE_SELECTED);
    for (int bone_index=0; bone_index<es->bone_count; bone_index++)
    {
        if (!IsBoneSelected(bone_index))
            continue;

        DrawEditorAnimationBone(en, bone_index, ea.position);
    }

    DrawTimeline();

    if (g_animation_view.state_draw)
        g_animation_view.state_draw();
}

static void HandlePrevFrameCommand()
{
    EditorAnimation* en = GetEditingAnimation();
    en->current_frame = (en->current_frame - 1 + en->frame_count) % en->frame_count;
    UpdateTransforms(en);
}

static void HandleNextFrameCommand()
{
    EditorAnimation* en = GetEditingAnimation();
    en->current_frame = (en->current_frame + 1) % en->frame_count;
    UpdateTransforms(en);
}

static void HandleMoveCommand()
{
    if (g_animation_view.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    if (GetEditingAnimation()->selected_bone_count <= 0)
        return;

    RecordUndo();
    SaveState();
    SetState(ANIMATION_VIEW_STATE_MOVE, UpdateMoveState, nullptr);
    SetCursor(SYSTEM_CURSOR_MOVE);
}

static void HandleRotate()
{
    if (g_animation_view.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    if (GetEditingAnimation()->selected_bone_count <= 0)
        return;

    RecordUndo();
    SaveState();
    SetState(ANIMATION_VIEW_STATE_ROTATE, UpdateRotateState, DrawRotateState);
}

static void HandleResetRotate()
{
    if (g_animation_view.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    RecordUndo();
    EditorAnimation* en = GetEditingAnimation();
    EditorSkeleton* es = GetEditingSkeleton();
    for (int bone_index=0; bone_index<es->bone_count; bone_index++)
    {
        if (!IsBoneSelected(bone_index))
            continue;

        SetRotation(GetFrameTransform(en, bone_index, en->current_frame), 0);
    }

    UpdateTransforms(en);
}

static void HandlePlayCommand()
{
    EditorAnimation* en = GetEditingAnimation();
    if (g_animation_view.state == ANIMATION_VIEW_STATE_PLAY)
    {
        Stop(en->animator);
        UpdateTransforms(en);
        SetState(ANIMATION_VIEW_STATE_DEFAULT, nullptr, nullptr);
        return;
    }

    if (g_animation_view.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    EditorAsset& ea = *GetEditingAsset();
    EditorSkeleton* es = GetEditingSkeleton();

    Init(
        en->animator,
        ToSkeleton(ALLOCATOR_DEFAULT, es, NAME_NONE));
    Play(en->animator, ToAnimation(ALLOCATOR_DEFAULT, en, ea.name), 1.0f, true);
    SetState(ANIMATION_VIEW_STATE_PLAY, UpdatePlayState, nullptr);
}

static void HandleResetMoveCommand()
{
    if (g_animation_view.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    RecordUndo();

    EditorAnimation* en = GetEditingAnimation();
    EditorSkeleton* es = GetEditingSkeleton();
    for (int bone_index=0; bone_index<es->bone_count; bone_index++)
    {
        if (!IsBoneSelected(bone_index))
            continue;

        SetPosition(GetFrameTransform(en, bone_index, en->current_frame), VEC2_ZERO);
    }

    UpdateTransforms(en);
}

static void HandleSelectAll()
{
    if (g_animation_view.state != ANIMATION_VIEW_STATE_DEFAULT)
        return;

    EditorSkeleton* es = GetEditingSkeleton();
    for (int i=0; i<es->bone_count; i++)
        AddSelection(i);
}

static void HandleInsertBeforeFrame()
{
    RecordUndo();
    EditorAnimation* en = GetEditingAnimation();
    en->current_frame = InsertFrame(en, en->current_frame);
    UpdateTransforms(en);
}

static void HandleInsertAfterFrame()
{
    RecordUndo();
    EditorAnimation* en = GetEditingAnimation();
    en->current_frame = InsertFrame(en, en->current_frame + 1);
    UpdateTransforms(en);
}

static void HandleDeleteFrame()
{
    RecordUndo();
    EditorAnimation* en = GetEditingAnimation();
    en->current_frame = DeleteFrame(en, en->current_frame);
    UpdateTransforms(en);
}

static void HandleToggleOnionSkin()
{
    g_animation_view.onion_skin = !g_animation_view.onion_skin;
}

void AnimationViewShutdown()
{
    EditorAnimation* en = GetEditingAnimation();
    Stop(en->animator);
    UpdateTransforms(en);
}

static void AddHoldFrame()
{
    EditorAnimation* en = GetEditingAnimation();
    RecordUndo();
    en->frames[en->current_frame].hold++;
    MarkModified();
}

static void RemoveHoldFrame()
{
    EditorAnimation* en = GetEditingAnimation();
    if (en->frames[en->current_frame].hold <= 0)
        return;

    RecordUndo();
    en->frames[en->current_frame].hold = Max(0, en->frames[en->current_frame].hold - 1);
    MarkModified();
}

void AnimationViewInit()
{
    g_view.vtable = {
        .update = AnimationViewUpdate,
        .draw = AnimationViewDraw,
        .shutdown = AnimationViewShutdown,
    };

    g_animation_view.state = ANIMATION_VIEW_STATE_DEFAULT;
    g_animation_view.state_update = nullptr;
    g_animation_view.state_draw = nullptr;

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
            { KEY_O, true, false, false, HandleToggleOnionSkin },
            { KEY_X, false, false, false, HandleDeleteFrame },
            { KEY_H, false, false, false, AddHoldFrame },
            { KEY_H, false, true, false, RemoveHoldFrame },
            { INPUT_CODE_NONE }
        };

        g_animation_editor_shortcuts = shortcuts;
        EnableShortcuts(g_animation_editor_shortcuts);
    }
}
