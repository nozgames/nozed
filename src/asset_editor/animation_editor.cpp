//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"

constexpr float FRAME_LINE_SIZE = 0.5f;
constexpr float FRAME_LINE_OFFSET = -0.1f;
constexpr float FRAME_SIZE = 0.16f;
constexpr float FRAME_SELECTED_SIZE = 0.32f;

enum AnimationEditorState
{
    ANIMATION_EDITOR_STATE_DEFAULT,
    ANIMATION_EDITOR_STATE_MOVE
};

struct SavedBone
{
    Mat3 world_to_local;
    Vec2 world_position;
    float rotation;
};

struct AnimationEditor
{
    AnimationEditorState state;
    EditorAsset* asset;
    EditorAnimation* animation;
    int selected_bone_count;
    bool clear_selection_on_up;
    int selected_frame;
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
        center += eb.local_to_world * VEC2_ZERO;
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
    }

    UpdateSelectionCenter();
}

static void SetState(AnimationEditorState state, void (*state_update)(), void (*state_draw)())
{
    g_animation_editor.state = state;
    g_animation_editor.state_update = state_update;
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

    EditorSkeleton& es = *en.skeleton_asset->skeleton;

    int bone_index = HitTestBone(
        es,
        ScreenToWorld(g_asset_editor.camera, GetMousePosition()) - g_animation_editor.asset->position);

    if (bone_index == -1)
        return false;

    SelectBone(bone_index);
    return true;
}

static void UpdateMoveState()
{
    EditorAnimation& en = *g_animation_editor.animation;
    if (!en.skeleton_asset)
        return;

    EditorSkeleton& es = *en.skeleton_asset->skeleton;

    Vec2 world_delta = g_asset_editor.mouse_world_position - g_animation_editor.command_world_position;

    for (int i=0; i<es.bone_count; i++)
    {
        EditorBone& eb = es.bones[i];
        if (!eb.selected)
            continue;

        SavedBone& sb = g_animation_editor.saved_bones[i];
        Vec2 bone_position = sb.world_to_local * (sb.world_position + world_delta);
        eb.position = bone_position;
    }

    UpdateTransforms(es);
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
        // UpdateSelection(ea);
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

    // Draw bone joints
    for (int i=0; i<es.bone_count; i++)
    {
        const EditorBone& bone = es.bones[i];
        Vec2 bone_position = bone.local_to_world * VEC2_ZERO;
        BindColor(bone.selected ? COLOR_SELECTED : COLOR_BLACK);
        DrawVertex(bone_position + ea.position);
    }
}

void DrawAnimationEditor()
{
    DrawSkeleton();

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
    DrawVertex({pos.x - left.x + h1.x * g_animation_editor.selected_frame, pos.y}, FRAME_SELECTED_SIZE);
}

static void HandlePrevFrameCommand()
{
    g_animation_editor.selected_frame = Max(0, g_animation_editor.selected_frame - 1);
}

static void HandleNextFrameCommand()
{
    g_animation_editor.selected_frame = Min(g_animation_editor.animation->frame_count - 1, g_animation_editor.selected_frame + 1);
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

static Shortcut g_animation_editor_shortcuts[] = {
    { KEY_G, false, false, false, HandleMoveCommand },
    // { KEY_P, false, false, false, HandleParentCommand },
    // { KEY_P, false, true, false, HandleUnparentCommand },
    { KEY_A, false, false, false, HandlePrevFrameCommand },
    { KEY_D, false, false, false, HandleNextFrameCommand },
    { INPUT_CODE_NONE }
};

void InitAnimationEditor(EditorAsset& ea)
{
    g_animation_editor.state = ANIMATION_EDITOR_STATE_DEFAULT;
    g_animation_editor.asset = &ea;
    g_animation_editor.animation = ea.anim;
    g_animation_editor.selected_frame = 0;

    EnableShortcuts(g_animation_editor_shortcuts);
}

