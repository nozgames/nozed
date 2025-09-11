//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"

constexpr float CENTER_SIZE = 0.2f;
constexpr float ORIGIN_SIZE = 0.1f;
constexpr float ORIGIN_BORDER_SIZE = 0.12f;
constexpr float ROTATE_TOOL_WIDTH = 0.02f;

enum SkeletonEditorState
{
    SKELETON_EDITOR_STATE_DEFAULT,
    SKELETON_EDITOR_STATE_MOVE,
    SKELETON_EDITOR_STATE_ROTATE,
    SKELETON_EDITOR_STATE_SCALE,
    SKELETON_EDITOR_STATE_PARENT
};

struct SavedBone
{
    Mat3 world_to_local;
    Vec2 world_position;
    float rotation;
};

struct SkeletonEditor
{
    SkeletonEditorState state;
    EditorAsset* asset;
    EditorSkeleton* skeleton;
    bool clear_selection_on_up;
    int selected_bone_count;
    Vec2 command_world_position;
    SavedBone saved_bones[MAX_BONES];
    Vec2 selection_center;
    Vec2 selection_center_world;
};

static SkeletonEditor g_skeleton_editor = {};
extern Shortcut g_skeleton_editor_shortcuts[];

static int GetFirstSelectedBoneIndex()
{
    EditorSkeleton& es = *g_skeleton_editor.skeleton;
    for (int i=0; i<es.bone_count; i++)
        if (es.bones[i].selected)
            return i;
    return -1;
}

static void UpdateSelectionCenter()
{
    Vec2 center = VEC2_ZERO;
    float center_count = 0.0f;
    for (int i=0; i<g_skeleton_editor.skeleton->bone_count; i++)
    {
        EditorBone& eb = g_skeleton_editor.skeleton->bones[i];
        if (!eb.selected)
            continue;
        center += eb.local_to_world * VEC2_ZERO;
        center_count += 1.0f;
    }

    g_skeleton_editor.selection_center =
        center_count < F32_EPSILON
            ? center
            : center / center_count;
    g_skeleton_editor.selection_center_world = g_skeleton_editor.selection_center + g_skeleton_editor.asset->position;
}

static void SaveState()
{
    for (int i=1; i<g_skeleton_editor.skeleton->bone_count; i++)
    {
        EditorBone& eb = g_skeleton_editor.skeleton->bones[i];
        SavedBone& sb = g_skeleton_editor.saved_bones[i];
        sb.world_to_local = g_skeleton_editor.skeleton->bones[eb.parent_index].world_to_local;
        sb.world_position = eb.local_to_world * VEC2_ZERO;
    }

    UpdateSelectionCenter();
}

static void SetState(SkeletonEditorState state)
{
    g_skeleton_editor.state = state;
    g_skeleton_editor.command_world_position = g_asset_editor.mouse_world_position;

    SetCursor(SYSTEM_CURSOR_DEFAULT);

    switch (state)
    {
    case SKELETON_EDITOR_STATE_PARENT:
    {
        SetCursor(SYSTEM_CURSOR_SELECT);
        break;
    }

    case SKELETON_EDITOR_STATE_MOVE:
    {
        RecordUndo(*g_skeleton_editor.asset);
        SaveState();
        break;
    }

    case SKELETON_EDITOR_STATE_ROTATE:
    {
        RecordUndo(*g_skeleton_editor.asset);
        SaveState();
        break;
    }

    default:
        break;
    }
}

static void ClearSelection()
{
    EditorSkeleton& es = *g_skeleton_editor.skeleton;
    for (int i=0; i<es.bone_count; i++)
        es.bones[i].selected = false;

    g_skeleton_editor.selected_bone_count = 0;
}

static bool SelectBone()
{
    EditorSkeleton& es = *g_skeleton_editor.skeleton;
    int bone_index = HitTestBone(
        es,
        ScreenToWorld(g_asset_editor.camera, GetMousePosition()) - g_skeleton_editor.asset->position);

    if (bone_index == -1)
        return false;

    ClearSelection();

    es.bones[bone_index].selected = true;
    g_skeleton_editor.selected_bone_count++;

    // if (IsCtrlDown(g_asset_editor.input) || IsShiftDown(g_asset_editor.input))
    //     ToggleSelection(em, vertex_index);
    // else
    //     SetSelection(em, vertex_index);
    //UpdateSelection(ea);

    return true;
}

static void UpdateDefaultState()
{
    EditorSkeleton& es = *g_skeleton_editor.skeleton;

    // If a drag has started then switch to box select
    if (g_asset_editor.drag)
    {
        //BeginBoxSelect(HandleBoxSelect);
        return;
    }

    // Select
    if (!g_asset_editor.drag && WasButtonReleased(g_asset_editor.input, MOUSE_LEFT))
    {
        g_skeleton_editor.clear_selection_on_up = false;

        if (SelectBone())
            return;

        g_skeleton_editor.clear_selection_on_up = true;
    }

    if (WasButtonReleased(g_asset_editor.input, MOUSE_LEFT) && g_skeleton_editor.clear_selection_on_up)
    {
        // ClearSelection(em);
        // UpdateSelection(ea);
    }
}

static void UpdateRotateState()
{
    Vec2 dir_start = Normalize(g_skeleton_editor.command_world_position - g_skeleton_editor.selection_center_world);
    Vec2 dir_current = Normalize(g_asset_editor.mouse_world_position - g_skeleton_editor.selection_center_world);
    float angle = SignedAngleDelta(dir_start, dir_current);
    if (fabsf(angle) < F32_EPSILON)
        return;

    EditorSkeleton& es = *g_skeleton_editor.skeleton;
    for (int i=0; i<es.bone_count; i++)
    {
        EditorBone& eb = es.bones[i];
        if (!eb.selected)
            continue;

        SavedBone& sb = g_skeleton_editor.saved_bones[i];
    }

    UpdateTransforms(es);
    UpdateSelectionCenter();
}

static void UpdateMoveState()
{
    Vec2 world_delta = g_asset_editor.mouse_world_position - g_skeleton_editor.command_world_position;

    EditorSkeleton& es = *g_skeleton_editor.asset->skeleton;
    for (int i=0; i<es.bone_count; i++)
    {
        EditorBone& eb = es.bones[i];
        if (!eb.selected)
            continue;

        SavedBone& sb = g_skeleton_editor.saved_bones[i];
        Vec2 bone_position = sb.world_to_local * (sb.world_position + world_delta);
        eb.position = bone_position;
    }

    UpdateTransforms(es);
}

static void UpdateParentState()
{
    if (WasButtonPressed(g_asset_editor.input, MOUSE_LEFT))
    {
        int asset_index = HitTestAssets(g_asset_editor.mouse_world_position);
        if (asset_index == -1)
            return;

        EditorAsset& hit_asset = *g_asset_editor.assets[asset_index];
        EditorSkeleton& es = *g_skeleton_editor.skeleton;
        es.skinned_meshes[es.skinned_mesh_count++] = {
            hit_asset.name,
            asset_index,
            GetFirstSelectedBoneIndex()
        };
    }
}

void UpdateSkeletonEditor()
{
    CheckShortcuts(g_skeleton_editor_shortcuts);

    switch (g_skeleton_editor.state)
    {
    case SKELETON_EDITOR_STATE_DEFAULT:
        UpdateDefaultState();
        return;

    case SKELETON_EDITOR_STATE_MOVE:
        UpdateMoveState();
        break;

    case SKELETON_EDITOR_STATE_ROTATE:
        UpdateRotateState();
        break;

    case SKELETON_EDITOR_STATE_PARENT:
        UpdateParentState();
        break;

    default:
        break;
    }

    // Commit the tool
    if (WasButtonPressed(g_asset_editor.input, MOUSE_LEFT) || WasButtonPressed(g_asset_editor.input, KEY_ENTER))
    {
        g_skeleton_editor.asset->modified = true;
        SetState(SKELETON_EDITOR_STATE_DEFAULT);
    }
    // Cancel the tool
    else if (WasButtonPressed(g_asset_editor.input, KEY_ESCAPE) || WasButtonPressed(g_asset_editor.input, MOUSE_RIGHT))
    {
        CancelUndo();
        SetState(SKELETON_EDITOR_STATE_DEFAULT);
    }
}

static void DrawRotateState()
{
    BindColor(SetAlpha(COLOR_CENTER, 0.75f));
    DrawVertex(g_skeleton_editor.selection_center_world, CENTER_SIZE * 0.75f);
    BindColor(COLOR_CENTER);
    DrawLine(g_asset_editor.mouse_world_position, g_skeleton_editor.selection_center_world);
    BindColor(COLOR_ORIGIN);
    DrawVertex(g_asset_editor.mouse_world_position, CENTER_SIZE);
}

static void DrawSkeleton()
{
    EditorAsset& ea = *g_skeleton_editor.asset;
    EditorSkeleton& es = *g_skeleton_editor.skeleton;

    for (int i=0; i<es.bone_count; i++)
    {
        const EditorBone& bone = es.bones[i];
        Vec2 bone_position = bone.local_to_world * VEC2_ZERO;
        BindColor(bone.selected ? COLOR_SELECTED : COLOR_BLACK);
        DrawVertex(bone_position + ea.position);
    }


    for (int i=0; i<es.bone_count; i++)
    {
        const EditorBone& bone = es.bones[i];
        BindColor(bone.selected ? COLOR_SELECTED : COLOR_BLACK);
        DrawVertex(bone.local_to_world * VEC2_ZERO + ea.position);
    }
}

void DrawSkeletonEditor()
{
    DrawSkeleton();

    switch (g_skeleton_editor.state)
    {
    case SKELETON_EDITOR_STATE_ROTATE:
        DrawRotateState();
        break;

    default:
        break;
    }
}

static void HandleMoveCommand()
{
    if (g_skeleton_editor.state != SKELETON_EDITOR_STATE_DEFAULT)
        return;

    if (g_skeleton_editor.selected_bone_count <= 0)
        return;

    SetState(SKELETON_EDITOR_STATE_MOVE);
}

static void HandleRotateCommand()
{
    if (g_skeleton_editor.state != SKELETON_EDITOR_STATE_DEFAULT)
        return;

    if (g_skeleton_editor.selected_bone_count <= 0)
        return;

    SetState(SKELETON_EDITOR_STATE_ROTATE);
}

static void HandleParentCommand()
{
    SetState(SKELETON_EDITOR_STATE_PARENT);
}

static Shortcut g_skeleton_editor_shortcuts[] = {
    { KEY_G, false, false, false, HandleMoveCommand },
    { KEY_R, false, false, false, HandleRotateCommand },
    { KEY_P, false, false, false, HandleParentCommand },
    { INPUT_CODE_NONE }
};

void InitSkeletonEditor(EditorAsset& ea)
{
    g_skeleton_editor.state = SKELETON_EDITOR_STATE_DEFAULT;
    g_skeleton_editor.asset = &ea;
    g_skeleton_editor.skeleton = ea.skeleton;

    EnableShortcuts(g_skeleton_editor_shortcuts);
}

