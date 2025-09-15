//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include <editor.h>

constexpr float CENTER_SIZE = 0.2f;
constexpr float ORIGIN_SIZE = 0.1f;
constexpr float ORIGIN_BORDER_SIZE = 0.12f;
constexpr float ROTATE_TOOL_WIDTH = 0.02f;
constexpr float BONE_ORIGIN_SIZE = 0.16f;

enum SkeletonEditorState
{
    SKELETON_EDITOR_STATE_DEFAULT,
    SKELETON_EDITOR_STATE_MOVE,
    SKELETON_EDITOR_STATE_ROTATE,
    SKELETON_EDITOR_STATE_SCALE,
    SKELETON_EDITOR_STATE_PARENT,
    SKELETON_EDITOR_STATE_UNPARENT
};

struct SkeletonViewBone
{
    BoneTransform transform;
    Mat3 local_to_world;
    Mat3 world_to_local;
    bool selected;
};

struct SkeletonView
{
    SkeletonEditorState state;
    void (*state_update)();
    void (*state_draw)();
    EditorAsset* asset;
    EditorSkeleton* skeleton;
    bool clear_selection_on_up;
    bool ignore_up;
    int selected_bone_count;
    Vec2 command_world_position;
    SkeletonViewBone bones[MAX_BONES];
    Vec2 selection_center;
    Vec2 selection_center_world;
    Shortcut* shortcuts;
};

static SkeletonView g_skeleton_view = {};

static EditorAsset& GetAsset() { return *g_skeleton_view.asset; }
static EditorSkeleton& GetSkeleton() { return *g_skeleton_view.skeleton; }
static bool IsBoneSelected(int bone_index) { return g_skeleton_view.bones[bone_index].selected; }
static void SetBoneSelected(int bone_index, bool selected)
{
    if (IsBoneSelected(bone_index) == selected)
        return;
    g_skeleton_view.bones[bone_index].selected = selected;
    g_skeleton_view.selected_bone_count += selected ? 1 : -1;
}

static int GetFirstSelectedBoneIndex()
{
    EditorSkeleton& es = GetSkeleton();
    for (int i=0; i<es.bone_count; i++)
        if (IsBoneSelected(i))
            return i;
    return -1;
}

static void UpdateAssetNames()
{
    if (g_skeleton_view.state != SKELETON_EDITOR_STATE_DEFAULT)
        return;

    if (!IsAltDown(g_view.input))
        return;

    EditorAsset& ea = GetAsset();
    EditorSkeleton& es = GetSkeleton();
    for (u16 i=0; i<es.bone_count; i++)
    {
        BeginWorldCanvas(g_view.camera, ea.position + TransformPoint(es.bones[i].local_to_world, VEC2_ZERO), Vec2{6, 0});
        SetStyleSheet(g_assets.ui.view);
            BeginElement(g_names.asset_name_container);
                Label(es.bones[i].name->value, g_names.asset_name);
            EndElement();
        EndCanvas();
    }
}

static void UpdateSelectionCenter()
{
    Vec2 center = VEC2_ZERO;
    float center_count = 0.0f;
    for (int i=0; i<g_skeleton_view.skeleton->bone_count; i++)
    {
        EditorBone& eb = g_skeleton_view.skeleton->bones[i];
        if (!g_skeleton_view.bones[i].selected)
            continue;
        center += TransformPoint(eb.local_to_world);
        center_count += 1.0f;
    }

    g_skeleton_view.selection_center =
        center_count < F32_EPSILON
            ? center
            : center / center_count;
    g_skeleton_view.selection_center_world = g_skeleton_view.selection_center + g_skeleton_view.asset->position;
}

static void SaveState()
{
    for (int i=1; i<g_skeleton_view.skeleton->bone_count; i++)
    {
        EditorBone& eb = g_skeleton_view.skeleton->bones[i];
        SkeletonViewBone& sb = g_skeleton_view.bones[i];
        sb.local_to_world = eb.local_to_world;
        sb.world_to_local = eb.world_to_local;
        sb.transform = eb.transform;
    }

    UpdateSelectionCenter();
}

static void RevertToSavedState()
{
    for (int i=1; i<g_skeleton_view.skeleton->bone_count; i++)
    {
        EditorBone& eb = g_skeleton_view.skeleton->bones[i];
        SkeletonViewBone& sb = g_skeleton_view.bones[i];
        eb.transform = sb.transform;
        eb.local_to_world = sb.local_to_world;
        eb.world_to_local = sb.world_to_local;
    }

    UpdateSelectionCenter();
}


static void SetState(SkeletonEditorState state, void (*state_update)(), void (*state_draw)())
{
    g_skeleton_view.state = state;
    g_skeleton_view.state_update = state_update;
    g_skeleton_view.state_draw = state_draw;
    g_skeleton_view.command_world_position = g_view.mouse_world_position;

    SetCursor(SYSTEM_CURSOR_DEFAULT);
}

static void ClearSelection()
{
    EditorSkeleton& es = GetSkeleton();
    for (int bone_index=0; bone_index<es.bone_count; bone_index++)
        SetBoneSelected(bone_index, false);
}

static void SelectBone(int bone_index)
{
    ClearSelection();
    SetBoneSelected(bone_index, true);
}

static bool SelectBone()
{
    EditorSkeleton& es = *g_skeleton_view.skeleton;
    int bone_index = HitTestBone(
        es,
        ScreenToWorld(g_view.camera, GetMousePosition()) - g_skeleton_view.asset->position);

    if (bone_index == -1)
        return false;

    SelectBone(bone_index);
    return true;
}

static void UpdateDefaultState()
{
    // If a drag has started then switch to box select
    if (g_view.drag)
    {
        //BeginBoxSelect(HandleBoxSelect);
        return;
    }

    if (!g_skeleton_view.ignore_up && !g_view.drag && WasButtonReleased(g_view.input, MOUSE_LEFT))
    {
        g_skeleton_view.clear_selection_on_up = false;

        if (SelectBone())
            return;

        g_skeleton_view.clear_selection_on_up = true;
    }

    g_skeleton_view.ignore_up &= !WasButtonReleased(g_view.input, MOUSE_LEFT);

    if (WasButtonReleased(g_view.input, MOUSE_LEFT) && g_skeleton_view.clear_selection_on_up)
    {
        ClearSelection();
    }
}

static void UpdateRotateState()
{
    EditorSkeleton& es = *g_skeleton_view.skeleton;

    Vec2 dir_start = Normalize(g_skeleton_view.command_world_position - g_skeleton_view.selection_center_world);
    Vec2 dir_current = Normalize(g_view.mouse_world_position - g_skeleton_view.selection_center_world);
    float angle = SignedAngleDelta(dir_start, dir_current);
    if (fabsf(angle) < F32_EPSILON)
        return;

    for (int bone_index=0; bone_index<es.bone_count; bone_index++)
    {
        if (!IsBoneSelected(bone_index))
            continue;

        SkeletonViewBone& vb = g_skeleton_view.bones[bone_index];
        es.bones[bone_index].transform.rotation = vb.transform.rotation - angle;
    }

    UpdateTransforms(es);
}

static void UpdateMoveState()
{
    Vec2 world_delta = g_view.mouse_world_position - g_skeleton_view.command_world_position;

    EditorSkeleton& es = *g_skeleton_view.asset->skeleton;
    for (int bone_index=1; bone_index<es.bone_count; bone_index++)
    {
        if (!IsBoneSelected(bone_index))
            continue;

        EditorBone& eb = es.bones[bone_index];
        SkeletonViewBone& vb = g_skeleton_view.bones[bone_index];
        Vec2 world_moved = TransformPoint(vb.world_to_local, world_delta);
        eb.transform.position = vb.transform.position + TransformPoint(vb.world_to_local, world_moved);
    }

    UpdateTransforms(es);
}

static void UpdateParentState()
{
    if (WasButtonPressed(g_view.input, MOUSE_LEFT))
    {
        int asset_index = HitTestAssets(g_view.mouse_world_position);
        if (asset_index == -1)
            return;

        EditorAsset& hit_asset = *g_view.assets[asset_index];
        EditorSkeleton& es = *g_skeleton_view.skeleton;
        es.skinned_meshes[es.skinned_mesh_count++] = {
            hit_asset.name,
            asset_index,
            GetFirstSelectedBoneIndex()
        };

        MarkModified(*g_skeleton_view.asset);
    }
}

static void UpdateUnparentState()
{
    EditorSkeleton& es = *g_skeleton_view.skeleton;
    if (WasButtonPressed(g_view.input, MOUSE_LEFT))
    {
        for (int i=0; i<g_skeleton_view.skeleton->skinned_mesh_count; i++)
        {
            EditorSkinnedMesh& esm = g_skeleton_view.skeleton->skinned_meshes[i];
            Vec2 bone_position = TransformPoint(es.bones[esm.bone_index].local_to_world) + g_skeleton_view.asset->position;
            EditorAsset& skinned_mesh_asset = *g_view.assets[esm.asset_index];
            if (!HitTestAsset(skinned_mesh_asset, bone_position, g_view.mouse_world_position))
                continue;

            for (int j=i; j<es.skinned_mesh_count-1; j++)
                es.skinned_meshes[j] = es.skinned_meshes[j+1];

            es.skinned_mesh_count--;
            return;
        }

        MarkModified(*g_skeleton_view.asset);
    }
}

void UpdateSkeletonEditor()
{
    CheckShortcuts(g_skeleton_view.shortcuts);

    UpdateAssetNames();

    if (g_skeleton_view.state_update)
        g_skeleton_view.state_update();

    if (g_skeleton_view.state == SKELETON_EDITOR_STATE_DEFAULT)
    {
        UpdateDefaultState();
        return;
    }

    // Commit the tool
    if (WasButtonPressed(g_view.input, MOUSE_LEFT) || WasButtonPressed(g_view.input, KEY_ENTER))
    {
        g_skeleton_view.asset->modified = true;
        g_skeleton_view.ignore_up = true;
        SetState(SKELETON_EDITOR_STATE_DEFAULT, nullptr, nullptr);
    }
    // Cancel the tool
    else if (WasButtonPressed(g_view.input, KEY_ESCAPE) || WasButtonPressed(g_view.input, MOUSE_RIGHT))
    {
        CancelUndo();
        RevertToSavedState();
        SetState(SKELETON_EDITOR_STATE_DEFAULT, nullptr, nullptr);
    }
}

static void DrawRotateState()
{
    BindColor(SetAlpha(COLOR_CENTER, 0.75f));
    DrawVertex(g_skeleton_view.selection_center_world, CENTER_SIZE * 0.75f);
    BindColor(COLOR_CENTER);
    DrawDashedLine(g_view.mouse_world_position, g_skeleton_view.selection_center_world);
    BindColor(COLOR_ORIGIN);
    DrawVertex(g_view.mouse_world_position, CENTER_SIZE);
}

static void DrawSkeleton()
{
    EditorAsset& ea = *g_skeleton_view.asset;
    EditorSkeleton& es = *g_skeleton_view.skeleton;

    BindMaterial(g_view.vertex_material);
    BindColor(COLOR_WHITE);
    for (int bone_index=1; bone_index<es.bone_count; bone_index++)
    {
        if (!IsBoneSelected(bone_index))
            continue;

        DrawEditorSkeletonBone(es, bone_index, ea.position);
    }

    for (int bone_index=0; bone_index<es.bone_count; bone_index++)
    {
        EditorBone& bone = es.bones[bone_index];
        Vec2 bone_position = TransformPoint(bone.local_to_world);
        BindColor(IsBoneSelected(bone_index) ? COLOR_SELECTED : COLOR_BLACK);
        DrawVertex(bone_position + ea.position, BONE_ORIGIN_SIZE);
    }
}

void DrawSkeletonEditor()
{
    DrawSkeleton();

    if (g_skeleton_view.state_draw)
        g_skeleton_view.state_draw();
}

static void HandleMoveCommand()
{
    if (g_skeleton_view.state != SKELETON_EDITOR_STATE_DEFAULT)
        return;

    if (g_skeleton_view.selected_bone_count <= 0)
        return;

    RecordUndo(*g_skeleton_view.asset);
    SaveState();
    SetState(SKELETON_EDITOR_STATE_MOVE, UpdateMoveState, nullptr);
    SetCursor(SYSTEM_CURSOR_MOVE);
}

static void HandleRotate()
{
    if (g_skeleton_view.state != SKELETON_EDITOR_STATE_DEFAULT)
        return;

    if (g_skeleton_view.selected_bone_count <= 0)
        return;

    RecordUndo(*g_skeleton_view.asset);
    SaveState();
    SetState(SKELETON_EDITOR_STATE_ROTATE, UpdateRotateState, DrawRotateState);
}

static void HandleParentCommand()
{
    SetState(SKELETON_EDITOR_STATE_PARENT, UpdateParentState, nullptr);
    SetCursor(SYSTEM_CURSOR_SELECT);
}

static void HandleUnparentCommand()
{
    SetState(SKELETON_EDITOR_STATE_UNPARENT, UpdateUnparentState, nullptr);
    SetCursor(SYSTEM_CURSOR_SELECT);
}

static void HandleExtrudeCommand()
{
    if (g_skeleton_view.selected_bone_count != 1)
        return;

    EditorSkeleton& es = *g_skeleton_view.skeleton;
    if (es.bone_count >= MAX_BONES)
        return;

    int parent_bone_index = GetFirstSelectedBoneIndex();
    assert(parent_bone_index != -1);

    EditorBone& parent_bone = es.bones[parent_bone_index];

    es.bones[es.bone_count++] = {
        GetName("Bone"),
        parent_bone_index,
        parent_bone.transform,
        false
    };

    SelectBone(es.bone_count-1);
    HandleMoveCommand();
}

static void RenameBone(const Name* name)
{
    assert(name);
    assert(name != NAME_NONE);

    if (g_skeleton_view.selected_bone_count != 1)
    {
        LogError("can only rename a single selected bone");
        return;
    }

    EditorSkeleton& es = *g_skeleton_view.asset->skeleton;

    es.bones[GetFirstSelectedBoneIndex()].name = name;
}

static const Name* SkeletonViewCommandPreview(const Command& command)
{
    if (g_skeleton_view.selected_bone_count != 1)
        return NAME_NONE;

    if (command.name != g_names.rename && command.name != g_names.r)
        return NAME_NONE;

    EditorSkeleton& es = *g_skeleton_view.asset->skeleton;
    if (command.arg_count <= 0)
        return es.bones[GetFirstSelectedBoneIndex()].name;

    return NAME_NONE;
}

static ViewVtable g_skeleton_view_vtable = {
    .rename = RenameBone,
    .preview_command = SkeletonViewCommandPreview
};

void InitSkeletonEditor(EditorAsset& ea)
{
    g_skeleton_view.state = SKELETON_EDITOR_STATE_DEFAULT;
    g_skeleton_view.asset = &ea;
    g_skeleton_view.skeleton = ea.skeleton;
    g_view.vtable = &g_skeleton_view_vtable;

    if (!g_skeleton_view.shortcuts)
    {
        static Shortcut shortcuts[] = {
            { KEY_G, false, false, false, HandleMoveCommand },
            { KEY_P, false, false, false, HandleParentCommand },
            { KEY_P, false, true, false, HandleUnparentCommand },
            { KEY_E, false, false, false, HandleExtrudeCommand },
            { KEY_R, false, false, false, HandleRotate },
            { INPUT_CODE_NONE }
        };

        g_skeleton_view.shortcuts = shortcuts;
        EnableShortcuts(g_skeleton_view.shortcuts);
    }
}

