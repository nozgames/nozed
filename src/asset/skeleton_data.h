//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#pragma  once

constexpr int MAX_SKINNED_MESHES = 64;

struct SkinnedMesh {
    const Name* asset_name;
    MeshData* mesh;
    int bone_index;
};

struct BoneData {
    const Name* name;
    int index;
    int parent_index;
    BoneTransform transform;
    Mat3 local_to_world;
    Mat3 world_to_local;
    float length;

    BoneTransform saved_transform;
    float saved_length;
    bool selected;
};

struct SkeletonData : AssetData {
    int bone_count;
    BoneData bones[MAX_BONES];
    SkinnedMesh skinned_meshes[MAX_SKINNED_MESHES];
    int skinned_mesh_count;
    int selected_bone_count;
    float opacity;
};

inline SkeletonData* GetEditorSkeleton(int index) {
    AssetData* ea = GetAssetData(index);
    assert(ea->type == ASSET_TYPE_SKELETON);
    return (SkeletonData*)ea;
}

extern void InitSkeletonData(AssetData* ea);
extern AssetData* NewEditorSkeleton(const std::filesystem::path& path);
extern AssetData* CreateEditorSkeletonAsset(const std::filesystem::path& path, SkeletonData* skeleton);
extern void DrawSkeletonData(SkeletonData* es, const Vec2& position);
extern void DrawEditorSkeletonBone(SkeletonData* s, int bone_index, const Vec2& position);
extern int HitTestBone(SkeletonData* s, const Vec2& world_pos);
extern void UpdateTransforms(SkeletonData* s);
extern void PostLoadEditorAssets(SkeletonData* es);
extern int FindBoneIndex(SkeletonData* s, const Name* name);
extern void Serialize(SkeletonData* s, Stream* stream);
extern Skeleton* ToSkeleton(Allocator* allocator, SkeletonData* es, const Name* name);
extern int ReparentBone(SkeletonData* s, int bone_index, int parent_index);
extern const Name* GetUniqueBoneName(SkeletonData* s);
extern void RemoveBone(SkeletonData* s, int bone_index);
extern void SortSkin(SkeletonData* s);

inline BoneData* GetParent(SkeletonData* es, BoneData* eb) {
    return eb->parent_index >= 0 ? &es->bones[eb->parent_index] : nullptr;
}

inline Mat3 GetParentLocalToWorld(SkeletonData* es, BoneData* eb, const Mat3& default_local_to_world) {
    return eb->parent_index >= 0 ? es->bones[eb->parent_index].local_to_world : default_local_to_world;
}
