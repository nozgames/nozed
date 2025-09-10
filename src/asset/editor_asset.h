//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

#include "../asset/editor_mesh.h"
#include "../asset/editor_vfx.h"

enum EditableAssetType
{
    EDITABLE_ASSET_TYPE_UNKNOWN = -1,
    EDITABLE_ASSET_TYPE_MESH,
    EDITABLE_ASSET_TYPE_VFX,
    EDITABLE_ASSET_TYPE_COUNT,
};

struct EditorAsset
{
    const Name* name;
    EditableAssetType type;
    union
    {
        EditorMesh* mesh;
        EditorVfx* vfx;
    };
    Vec2 position;
    Vec2 saved_position;
    bool dirty;
    char path[1024];
    bool selected;
    VfxHandle vfx_handle;
};


extern void HotloadEditorAsset(const Name* name);