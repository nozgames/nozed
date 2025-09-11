//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

#include "editor_mesh.h"
#include "editor_vfx.h"
#include "editor_skeleton.h"

enum EditorAssetType
{
    EDITOR_ASSET_TYPE_UNKNOWN = -1,
    EDITOR_ASSET_TYPE_MESH,
    EDITOR_ASSET_TYPE_VFX,
    EDITOR_ASSET_TYPE_SKELETON,
    EDITOR_ASSET_TYPE_COUNT,
};

struct EditorAsset
{
    EditorAssetType type;
    const Name* name;
    char path[1024];
    union
    {
        EditorMesh* mesh;
        EditorVfx* vfx;
        EditorSkeleton* skeleton;
    };
    Vec2 position;
    Vec2 saved_position;
    bool dirty;
    bool selected;
    VfxHandle vfx_handle;
    bool editing;
    bool modified;
};

extern void HotloadEditorAsset(const Name* name);
extern void MarkModified(EditorAsset& ea);
