//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

#include "asset/animation_editor.h"
#include "asset/mesh_editor.h"
#include "asset/skeleton_editor.h"
#include "asset/vfx_editor.h"

enum EditorAssetType
{
    EDITOR_ASSET_TYPE_UNKNOWN = -1,
    EDITOR_ASSET_TYPE_MESH,
    EDITOR_ASSET_TYPE_VFX,
    EDITOR_ASSET_TYPE_SKELETON,
    EDITOR_ASSET_TYPE_ANIMATION,
    EDITOR_ASSET_TYPE_COUNT,
};

typedef void (*EditorAssetRenameFunc) (const Name* new_name);

struct EditorAssetVtable
{
    void (*post_load)(EditorAsset& ea);
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
        EditorAnimation* anim;
    };
    Vec2 position;
    Vec2 saved_position;
    bool dirty;
    bool selected;
    VfxHandle vfx_handle;
    bool editing;
    bool modified;
    EditorAssetVtable vtable;
};

extern void HotloadEditorAsset(const Name* name);
extern void MarkModified(EditorAsset& ea);
extern EditorAsset* CreateEditorAsset(const std::filesystem::path& path, EditorAssetType type);
extern std::filesystem::path GetEditorAssetPath(const Name* name, const char* ext);
