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
    SKELETON_EDITOR_STATE_EXTRUDE,
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

static EditorSkeleton& GetEditingSkeleton() { return GetEditingAsset().skeleton; }
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
    EditorSkeleton& es = GetEditingSkeleton();
    for (int i=0; i<es.bone_count; i++)
        if (IsBoneSelected(i))
            return i;
    return -1;
}

static void UpdateAllAnimations(EditorAsset& ea)
{
    extern void UpdateSkeleton(EditorAnimation& en);

    for (u32 i=0; i<g_view.asset_count; i++)
    {
        EditorAsset* other = g_view.assets[i];
        if (other->type != EDITOR_ASSET_TYPE_ANIMATION)
            continue;

        EditorAnimation& en = other->animation;
        if (en.skeleton_asset_index == ea.index)
        {
            RecordUndo(*other);
            UpdateSkeleton(en);
            MarkModified(*other);
        }
    }
}

static void UpdateAssetNames()
{
    if (g_skeleton_view.state != SKELETON_EDITOR_STATE_DEFAULT)
        return;

    if (!IsAltDown(g_view.input) && !g_view.show_names)
        return;

    EditorAsset& ea = GetEditingAsset();
    EditorSkeleton& es = GetEditingSkeleton();
    for (u16 i=0; i<es.bone_count; i++)
    {
        Mat3 transform = es.bones[i].local_to_world * Rotate(es.bones[i].transform.rotation);
        Vec2 p = (TransformPoint(transform) + TransformPoint(transform, VEC2_RIGHT)) * 0.5f + ea.position;
        BeginWorldCanvas(g_view.camera, p, Vec2{6, 6}, nullptr, STYLESHEET_VIEW);
            BeginElement(NAME_ASSET_NAME_CONTAINER);
                Label(es.bones[i].name->value, NAME_ASSET_NAME);
            EndElement();
        EndCanvas();
    }
}

static void UpdateSelectionCenter()
{
    EditorAsset& ea = GetEditingAsset();
    EditorSkeleton& es = GetEditingSkeleton();

    Vec2 center = VEC2_ZERO;
    float center_count = 0.0f;
    for (int i=0; i<es.bone_count; i++)
    {
        EditorBone& eb = es.bones[i];
        if (!g_skeleton_view.bones[i].selected)
            continue;
        center += TransformPoint(eb.local_to_world);
        center_count += 1.0f;
    }

    g_skeleton_view.selection_center =
        center_count < F32_EPSILON
            ? center
            : center / center_count;
    g_skeleton_view.selection_center_world = g_skeleton_view.selection_center + ea.position;
}

static void SaveState()
{
    EditorSkeleton& es = GetEditingSkeleton();
    for (int i=0; i<es.bone_count; i++)
    {
        EditorBone& eb = es.bones[i];
        SkeletonViewBone& sb = g_skeleton_view.bones[i];
        sb.local_to_world = eb.local_to_world;
        sb.world_to_local = eb.world_to_local;
        sb.transform = eb.transform;
    }

    UpdateSelectionCenter();
}

static void RevertToSavedState()
{
    EditorSkeleton& es = GetEditingSkeleton();
    for (int i=0; i<es.bone_count; i++)
    {
        EditorBone& eb = es.bones[i];
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
    EditorSkeleton& es = GetEditingSkeleton();
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
    EditorAsset& ea = GetEditingAsset();
    EditorSkeleton& es = GetEditingSkeleton();
    int bone_index = HitTestBone(es, g_view.mouse_world_position - ea.position);
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
    EditorSkeleton& es = GetEditingSkeleton();

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
        es.bones[bone_index].transform.rotation = vb.transform.rotation + angle;
    }

    UpdateTransforms(es);
}

static void UpdateMoveState()
{
    EditorSkeleton& es = GetEditingSkeleton();
    for (int bone_index=0; bone_index<es.bone_count; bone_index++)
    {
        if (!IsBoneSelected(bone_index))
            continue;

        EditorBone& eb = es.bones[bone_index];
        SkeletonViewBone& vb = g_skeleton_view.bones[bone_index];
        Vec2 m0 = TransformPoint(vb.world_to_local, g_skeleton_view.command_world_position);
        Vec2 m1 = TransformPoint(vb.world_to_local, g_view.mouse_world_position);
        eb.transform.position = vb.transform.position + (m1 - m0);
    }

    UpdateTransforms(es);
}

static void UpdateParentState()
{
    if (!WasButtonPressed(g_view.input, MOUSE_LEFT))
        return;

    EditorAsset& ea = GetEditingAsset();
    EditorSkeleton& es = GetEditingSkeleton();

    // Bone?
    int bone_index = HitTestBone(
        es,
        g_view.mouse_world_position - ea.position);

    if (bone_index != -1)
    {
        BeginUndoGroup();
        RecordUndo(ea);
        bone_index = ReparentBone(es, GetFirstSelectedBoneIndex(), bone_index);
        SelectBone(bone_index);
        UpdateAllAnimations(ea);
        EndUndoGroup();
        return;
    }

    // Asset?
    int asset_index = HitTestAssets(g_view.mouse_world_position);
    if (asset_index == -1)
        return;

    RecordUndo(ea);
    EditorAsset& ea_hit = *g_view.assets[asset_index];
    es.skinned_meshes[es.skinned_mesh_count++] = {
        ea_hit.name,
        asset_index,
        GetFirstSelectedBoneIndex()
    };

    MarkModified();
}

static void UpdateUnparentState()
{
    if (!WasButtonPressed(g_view.input, MOUSE_LEFT))
        return;

    EditorAsset& ea = GetEditingAsset();
    EditorSkeleton& es = GetEditingSkeleton();
    for (int i=0; i<es.skinned_mesh_count; i++)
    {
        EditorSkinnedMesh& esm = es.skinned_meshes[i];
        Vec2 bone_position = TransformPoint(es.bones[esm.bone_index].local_to_world) + ea.position;
        EditorAsset& skinned_mesh_asset = *g_view.assets[esm.asset_index];
        if (!OverlapPoint(skinned_mesh_asset, bone_position, g_view.mouse_world_position))
            continue;

        RecordUndo(ea);
        for (int j=i; j<es.skinned_mesh_count-1; j++)
            es.skinned_meshes[j] = es.skinned_meshes[j+1];

        es.skinned_mesh_count--;
        return;
    }

    MarkModified();
}

void SkeletonViewUpdate()
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
        MarkModified();
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
    EditorAsset& ea = GetEditingAsset();
    EditorSkeleton& es = GetEditingSkeleton();

    BindMaterial(g_view.vertex_material);
    BindColor(COLOR_WHITE);
    for (int bone_index=0; bone_index<es.bone_count; bone_index++)
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

void SkeletonViewDraw()
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

    RecordUndo();
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

    RecordUndo();
    SaveState();
    SetState(SKELETON_EDITOR_STATE_ROTATE, UpdateRotateState, DrawRotateState);
}

static void HandleRemove()
{
    if (g_skeleton_view.state != SKELETON_EDITOR_STATE_DEFAULT)
        return;

    if (g_skeleton_view.selected_bone_count <= 0)
        return;

    EditorAsset& ea = GetEditingAsset();
    BeginUndoGroup();
    RecordUndo(ea);

    EditorSkeleton& es = GetEditingSkeleton();
    for (int i=es.bone_count - 1; i >=0; i--)
    {
        if (!IsBoneSelected(i))
            continue;

        RemoveBone(es, i);
    }

    UpdateAllAnimations(ea);
    EndUndoGroup();
    ClearSelection();
    MarkModified(GetEditingAsset());
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

    EditorSkeleton& es = GetEditingSkeleton();
    if (es.bone_count >= MAX_BONES)
        return;

    int parent_bone_index = GetFirstSelectedBoneIndex();
    assert(parent_bone_index != -1);

    RecordUndo();

    es.bones[es.bone_count] = {
        .name = GetUniqueBoneName(es),
        .index = es.bone_count,
        .parent_index = parent_bone_index,
        .transform = { .scale = VEC2_ONE }
    };
    es.bone_count++;

    UpdateTransforms(es);
    SelectBone(es.bone_count-1);
    SaveState();
    SetState(SKELETON_EDITOR_STATE_EXTRUDE, UpdateMoveState, nullptr);
    SetCursor(SYSTEM_CURSOR_MOVE);
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

    BeginUndoGroup();
    RecordUndo();
    GetEditingSkeleton().bones[GetFirstSelectedBoneIndex()].name = name;
    UpdateAllAnimations(GetEditingAsset());
    EndUndoGroup();
}

static const Name* SkeletonViewCommandPreview(const Command& command)
{
    if (g_skeleton_view.selected_bone_count != 1)
        return NAME_NONE;

    if (command.name != NAME_RENAME && command.name != NAME_R)
        return NAME_NONE;

    if (command.arg_count <= 0)
        return GetEditingSkeleton().bones[GetFirstSelectedBoneIndex()].name;

    return NAME_NONE;
}

void SkeletonViewInit()
{
    g_skeleton_view.state = SKELETON_EDITOR_STATE_DEFAULT;
    g_view.vtable = {
        .rename = RenameBone,
        .preview_command = SkeletonViewCommandPreview
    };

    if (!g_skeleton_view.shortcuts)
    {
        static Shortcut shortcuts[] = {
            { KEY_G, false, false, false, HandleMoveCommand },
            { KEY_P, false, false, false, HandleParentCommand },
            { KEY_P, false, true, false, HandleUnparentCommand },
            { KEY_E, false, false, false, HandleExtrudeCommand },
            { KEY_R, false, false, false, HandleRotate },
            { KEY_X, false, false, false, HandleRemove },
            { INPUT_CODE_NONE }
        };

        g_skeleton_view.shortcuts = shortcuts;
        EnableShortcuts(g_skeleton_view.shortcuts);
    }
}

