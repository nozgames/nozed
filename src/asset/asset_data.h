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
    bool post_loaded;
    bool editor_only;
    AssetVtable vtable;
    Bounds2 bounds;
    int sort_order;
    const AssetImporter* importer;
};

inline AssetData* GetAssetDataInternal(int index, AssetType type=ASSET_TYPE_UNKNOWN) {
    assert(index >= 0 && index < (int)MAX_ASSETS);
    if (!IsValid(g_editor.asset_allocator, index))
        return nullptr;
    AssetData* ea = (AssetData*)GetAt(g_editor.asset_allocator, index);
    assert(type == ASSET_TYPE_UNKNOWN || (ea && ea->type == type));
    return ea;
}

inline u32 GetAssetCount() {
    return GetCount(g_editor.asset_allocator);
}

extern AssetData* GetAssetData(AssetType type, const Name* name);
extern AssetData* GetAssetData(const std::filesystem::path& path);

inline AssetData* GetAssetData(u32 index) {
    assert(index < GetAssetCount());
    AssetData* a = GetAssetDataInternal(g_editor.sorted_assets[index]);
    assert(a);
    return a;
}

extern void InitAssetData();
extern void LoadAssetData(AssetData* a);
extern void PostLoadAssetData(AssetData* a);
extern void HotloadEditorAsset(const Name* name);
extern void MarkModified(AssetData* a);
inline void MarkModified() { MarkModified(GetAssetData()); }
extern void MarkMetaModified();
extern void MarkMetaModified(AssetData* a);
inline void MarkMetaModified() { MarkMetaModified(GetAssetData()); }
extern AssetData* CreateAssetData(const std::filesystem::path& path);
extern std::filesystem::path GetEditorAssetPath(const Name* name, const char* ext);
extern void Clone(AssetData* dst, AssetData* src);
extern void LoadAssetData();
extern void SaveAssetData();
extern void PostLoadAssetData();
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
extern bool InitImporter(AssetData* ea);
extern const Name* MakeCanonicalAssetName(const char* name);
extern const Name* MakeCanonicalAssetName(const std::filesystem::path& path);
extern void DeleteAsset(AssetData* ea);
extern void SortAssets(bool notify=true);
inline bool IsEditing(AssetData* a) { return a->editing; }

inline Bounds2 GetBounds(AssetData* a) { return a->bounds; }

#include "animation_data.h"
#include "mesh_data.h"
#include "texture_data.h"
#include "skeleton_data.h"
#include "vfx_data.h"

union FatAssetData {
    AssetData asset;
    MeshData mesh;
    TextureData texture;
    SkeletonData skeleton;
    VfxData vfx;
    AnimationData animation;
};
