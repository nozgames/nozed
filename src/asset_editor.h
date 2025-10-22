//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

enum EditorAssetType
{
    EDITOR_ASSET_TYPE_UNKNOWN = -1,
    EDITOR_ASSET_TYPE_MESH,
    EDITOR_ASSET_TYPE_VFX,
    EDITOR_ASSET_TYPE_SKELETON,
    EDITOR_ASSET_TYPE_ANIMATION,
    EDITOR_ASSET_TYPE_SOUND,
    EDITOR_ASSET_TYPE_TEXTURE,
    EDITOR_ASSET_TYPE_FONT,
    EDITOR_ASSET_TYPE_SHADER,
    EDITOR_ASSET_TYPE_COUNT,
};

typedef void (*EditorAssetRenameFunc) (const Name* new_name);

struct EditorAsset;

struct EditorAssetVtable
{
    void (*load)(EditorAsset* ea);
    void (*post_load)(EditorAsset* ea);
    void (*save)(EditorAsset* ea, const std::filesystem::path& path);
    void (*load_metadata)(EditorAsset* ea, Props* meta);
    void (*save_metadata)(EditorAsset* ea, Props* meta);
    void (*draw)(EditorAsset* ea);
    void (*view_init)();
    bool (*overlap_point)(EditorAsset* ea, const Vec2& position, const Vec2& hit_pos);
    bool (*overlap_bounds)(EditorAsset* ea, const Bounds2& hit_bounds);
    void (*clone)(EditorAsset* ea);
    void (*undo_redo)(EditorAsset* ea);
};

struct EditorAsset {
    EditorAssetType type;
    int asset_path_index;
    const Name* name;
    char path[1024];
    Vec2 position;
    Vec2 saved_position;
    bool selected;
    bool editing;
    bool modified;
    bool meta_modified;
    bool clipped;
    bool loaded;
    EditorAssetVtable vtable;
    Bounds2 bounds;
    int sort_order;
    const AssetImporter* importer;
};

inline EditorAsset* GetEditorAsset(int index, EditorAssetType type=EDITOR_ASSET_TYPE_UNKNOWN) {
    assert(index >= 0 && index < (int)MAX_ASSETS);
    if (!IsValid(g_editor.asset_allocator, index))
        return nullptr;
    EditorAsset* ea = (EditorAsset*)GetAt(g_editor.asset_allocator, index);
    assert(type == EDITOR_ASSET_TYPE_UNKNOWN || (ea && ea->type == type));
    return ea;
}

inline u32 GetEditorAssetCount() {
    return GetCount(g_editor.asset_allocator);
}

inline int GetIndex(EditorAsset* ea) {
    return (int)GetIndex(g_editor.asset_allocator, ea);
}

extern void InitEditorAssets();
extern void LoadEditorAsset(EditorAsset* ea);
extern void HotloadEditorAsset(const Name* name);
extern void MarkModified();
extern void MarkModified(EditorAsset* ea);
extern EditorAsset* CreateEditorAsset(const std::filesystem::path& path);
extern std::filesystem::path GetEditorAssetPath(const Name* name, const char* ext);
extern void Clone(EditorAsset* dst, EditorAsset* src);
extern void LoadEditorAssets();
extern void SaveEditorAssets();
extern bool OverlapPoint(EditorAsset* ea, const Vec2& overlap_point);
extern bool OverlapPoint(EditorAsset* ea, const Vec2& position, const Vec2& overlap_point);
extern bool OverlapBounds(EditorAsset* ea, const Bounds2& overlap_bounds);
extern int HitTestAssets(const Vec2& overlap_point);
extern int HitTestAssets(const Bounds2& bit_bounds);
extern void DrawAsset(EditorAsset* ea);
extern Bounds2 GetBounds(EditorAsset* ea);
extern int GetFirstSelectedAsset();
extern void MoveTo(EditorAsset* ea, const Vec2& position);
extern void ClearAssetSelection();
extern void SetAssetSelection(int asset_index);
extern void AddAssetSelection(int asset_index);
extern void ToggleAssetSelection(int asset_index);
extern EditorAsset* GetEditorAsset(EditorAssetType type, const Name* name);
extern EditorAsset* GetEditorAsset(const std::filesystem::path& path);
extern bool InitImporter(EditorAsset* ea);
extern const Name* MakeCanonicalAssetName(const char* name);
extern const Name* MakeCanonicalAssetName(const std::filesystem::path& path);
extern void DeleteEditorAsset(EditorAsset* ea);


#include "asset/animation_editor.h"
#include "asset/mesh_editor.h"
#include "asset/skeleton_editor.h"
#include "asset/vfx_editor.h"

union EditorAssetData
{
    EditorAsset asset;
    EditorMesh mesh;
    EditorVfx vfx;
    EditorSkeleton skeleton;
    EditorAnimation animation;
};

