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
    void (*draw)(EditorAsset& ea);
    void (*clone)(Allocator* allocator, const EditorAsset& ea, EditorAsset& clone);
    void (*init_editor)(EditorAsset& ea);
    void (*update_editor)();
    void (*draw_editor)();
    void (*shutdown_editor)();
    void (*save_metadata)(const EditorAsset& ea, Props* meta);
    bool (*overlap_point)(EditorAsset& ea, const Vec2& position, const Vec2& hit_pos);
    bool (*overlap_bounds)(EditorAsset& ea, const Bounds2& hit_bounds);
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
    bool selected;
    VfxHandle vfx_handle;
    bool editing;
    bool modified;
    bool meta_modified;
    EditorAssetVtable vtable;
};

extern void HotloadEditorAsset(const Name* name);
extern void MarkModified(EditorAsset& ea);
extern EditorAsset* CreateEditorAsset(const std::filesystem::path& path, EditorAssetType type);
extern std::filesystem::path GetEditorAssetPath(const Name* name, const char* ext);
extern EditorAsset* LoadEditorMeshAsset(const std::filesystem::path& path);
extern EditorAsset* LoadEditorVfxAsset(const std::filesystem::path& path);
extern void LoadEditorAssets();
extern void SaveEditorAssets();
extern bool OverlapPoint(EditorAsset& ea, const Vec2& overlap_point);
extern bool OverlapPoint(EditorAsset& ea, const Vec2& position, const Vec2& overlap_point);
extern bool OverlapBounds(EditorAsset& ea, const Bounds2& overlap_bounds);
extern int HitTestAssets(const Vec2& overlap_point);
extern int HitTestAssets(const Bounds2& bit_bounds);
extern void DrawEdges(const EditorAsset& ea, int min_edge_count, Color color);
extern void DrawAsset(EditorAsset& ea);
extern Bounds2 GetBounds(const EditorAsset& ea);
extern int GetFirstSelectedAsset();
extern Bounds2 GetSelectedBounds(const EditorAsset& ea);
extern void MoveTo(EditorAsset& asset, const Vec2& position);
extern void ClearAssetSelection();
extern void SetAssetSelection(int asset_index);
extern void AddAssetSelection(int asset_index);
extern int FindEditorAssetByName(const Name* name);
extern EditorAsset* Clone(Allocator* allocator, const EditorAsset& ea);
extern void Copy(EditorAsset& dst, const EditorAsset& src);
extern EditorAsset* GetEditorAsset(i32 index);
