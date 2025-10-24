//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

struct AssetData;

struct AssetVtable {
    void (*load)(AssetData* ea);
    void (*post_load)(AssetData* ea);
    void (*save)(AssetData* ea, const std::filesystem::path& path);
    void (*load_metadata)(AssetData* ea, Props* meta);
    void (*save_metadata)(AssetData* ea, Props* meta);
    void (*draw)(AssetData* ea);
    bool (*overlap_point)(AssetData* ea, const Vec2& position, const Vec2& hit_pos);
    bool (*overlap_bounds)(AssetData* ea, const Bounds2& hit_bounds);
    void (*clone)(AssetData* ea);
    void (*undo_redo)(AssetData* ea);
    void (*on_sort_order_changed)(AssetData* ea);

    void (*editor_begin)();
    void (*editor_end)();
    void (*editor_update)();
    void (*editor_draw)();
    Bounds2 (*editor_bounds)();
};

struct AssetData {
    AssetType type;
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
    AssetVtable vtable;
    Bounds2 bounds;
    int sort_order;
    const AssetImporter* importer;
};

inline AssetData* GetAssetData(int index, AssetType type=ASSET_TYPE_UNKNOWN) {
    assert(index >= 0 && index < (int)MAX_ASSETS);
    if (!IsValid(g_editor.asset_allocator, index))
        return nullptr;
    AssetData* ea = (AssetData*)GetAt(g_editor.asset_allocator, index);
    assert(type == ASSET_TYPE_UNKNOWN || (ea && ea->type == type));
    return ea;
}

inline AssetData* GetSortedAssetData(int index) { return GetAssetData(g_editor.sorted_assets[index]); }

inline u32 GetAssetCount() {
    return GetCount(g_editor.asset_allocator);
}

inline int GetIndex(AssetData* ea) {
    return (int)GetIndex(g_editor.asset_allocator, ea);
}

extern void InitEditorAssets();
extern void LoadEditorAsset(AssetData* ea);
extern void HotloadEditorAsset(const Name* name);
extern void MarkModified();
extern void MarkModified(AssetData* ea);
extern AssetData* CreateEditorAsset(const std::filesystem::path& path);
extern std::filesystem::path GetEditorAssetPath(const Name* name, const char* ext);
extern void Clone(AssetData* dst, AssetData* src);
extern void LoadEditorAssets();
extern void SaveEditorAssets();
extern bool OverlapPoint(AssetData* ea, const Vec2& overlap_point);
extern bool OverlapPoint(AssetData* ea, const Vec2& position, const Vec2& overlap_point);
extern bool OverlapBounds(AssetData* ea, const Bounds2& overlap_bounds);
extern AssetData* HitTestAssets(const Vec2& overlap_point);
extern AssetData* HitTestAssets(const Bounds2& bit_bounds);
extern void DrawAsset(AssetData* ea);
extern AssetData* GetFirstSelectedAsset();
extern void SetPosition(AssetData* ea, const Vec2& position);
extern void ClearAssetSelection();
extern void SetSelected(AssetData* a, bool selected);
extern void ToggleSelected(AssetData* a);
extern AssetData* GetAssetData(AssetType type, const Name* name);
extern AssetData* GetAssetData(const std::filesystem::path& path);
extern bool InitImporter(AssetData* ea);
extern const Name* MakeCanonicalAssetName(const char* name);
extern const Name* MakeCanonicalAssetName(const std::filesystem::path& path);
extern void DeleteAsset(AssetData* ea);
extern void SortAssets();

inline Bounds2 GetBounds(AssetData* a) { return a->bounds; }


#include "asset/animation_editor.h"
#include "asset/mesh_data.h"
#include "asset/skeleton_editor.h"
#include "asset/vfx_editor.h"

union FatAssetData {
    AssetData asset;
    MeshData mesh;
    EditorVfx vfx;
    EditorSkeleton skeleton;
    EditorAnimation animation;
};
