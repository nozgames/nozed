//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#pragma  once

constexpr int MAX_SKINNED_MESHES = 64;

struct EditorSkinnedMesh
{
    const Name* asset_name;
    int asset_index;
    int bone_index;
};

struct EditorBone
{
    const Name* name;
    int parent_index;
    Vec2 position;
    Mat3 local_to_world;
    Mat3 world_to_local;
    float length;
    bool selected;
};

struct EditorSkeleton
{
    int bone_count;
    EditorBone bones[MAX_BONES];
    Bounds2 bounds;
    EditorSkinnedMesh skinned_meshes[MAX_SKINNED_MESHES];
    int skinned_mesh_count;
};

extern EditorSkeleton* LoadEditorSkeleton(Allocator* allocator, const std::filesystem::path& path);
extern void DrawEditorSkeleton(EditorAsset& ea, bool selected);
extern void DrawEditorSkeleton(EditorAsset& ea, const Vec2& position, bool selected);
extern EditorAsset* NewEditorSkeleton(const std::filesystem::path& path);
extern EditorAsset* LoadEditorSkeletonAsset(const std::filesystem::path& path);
extern int HitTestBone(const EditorSkeleton& es, const Vec2& world_pos);
extern void UpdateTransforms(EditorSkeleton& es);
extern void SaveEditorSkeleton(const EditorSkeleton& es, const std::filesystem::path& path);
extern void SaveAssetMetadata(const EditorSkeleton& es, Props* meta);
extern void LoadAssetMetadata(EditorSkeleton& es, Props* meta);
extern void PostLoadEditorAssets(EditorSkeleton& es);
extern int FindBoneIndex(const EditorSkeleton& es, const Name* name);
