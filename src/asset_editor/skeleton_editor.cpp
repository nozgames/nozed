//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"

struct SkeletonEditor
{

};

static SkeletonEditor g_skeleton_editor = {};

static Shortcut g_skeleton_editor_shortcuts[] = {
    // { KEY_R, false, false, false, RotateEdges },
    { INPUT_CODE_NONE }
};

void InitSkeletonEditor(EditorAsset& ea)
{
    //g_mesh_editor.state = MESH_EDITOR_STATE_DEFAULT;

    EnableShortcuts(g_skeleton_editor_shortcuts);
}

