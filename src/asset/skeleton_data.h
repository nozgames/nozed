//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#pragma  once

struct BoneData {
    const Name* name;
    int index;
    int parent_index;
    BoneTransform transform;
    Mat3 local_to_world;
    Mat3 world_to_local;
    float length;
    bool selected;
};

struct RuntimeSkeletonData {
    BoneData bones[MAX_BONES];
    Skin skins[SKIN_MAX];
};

struct SkeletonData : AssetData {
    RuntimeSkeletonData* data;
    BoneData* bones;
    Skin* skins;
    int bone_count;
    int skin_count;
    int selected_bone_count;
    float opacity;
};

extern void InitSkeletonData(AssetData* a);
extern AssetData* NewEditorSkeleton(const std::filesystem::path& path);
extern AssetData* CreateEditorSkeletonAsset(const std::filesystem::path& path, SkeletonData* skeleton);
extern void DrawSkeletonData(SkeletonData* s, const Vec2& position);
extern void DrawEditorSkeletonBone(SkeletonData* s, int bone_index, const Vec2& position);
extern int HitTestBones(SkeletonData* s, const Mat3& transform, const Vec2& position, int* bones, int max_bones=MAX_BONES);
extern int HitTestBone(SkeletonData* s, const Vec2& world_pos);
extern int HitTestBone(SkeletonData* s, const Mat3& transform, const Vec2& world_pos);
extern void UpdateTransforms(SkeletonData* s);
extern void PostLoadEditorAssets(SkeletonData* s);
extern int FindBoneIndex(SkeletonData* s, const Name* name);
extern void Serialize(SkeletonData* s, Stream* stream);
extern Skeleton* ToSkeleton(Allocator* allocator, SkeletonData* s);
extern int ReparentBone(SkeletonData* s, int bone_index, int parent_index);
extern const Name* GetUniqueBoneName(SkeletonData* s);
extern void RemoveBone(SkeletonData* s, int bone_index);
extern int GetMirrorBone(SkeletonData* s, int bone_index);

inline BoneData* GetParent(SkeletonData* es, BoneData* eb) {
    return eb->parent_index >= 0 ? &es->bones[eb->parent_index] : nullptr;
}

inline Mat3 GetParentLocalToWorld(SkeletonData* es, BoneData* eb, const Mat3& default_local_to_world) {
    return eb->parent_index >= 0 ? es->bones[eb->parent_index].local_to_world : default_local_to_world;
}
