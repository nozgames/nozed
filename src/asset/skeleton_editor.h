//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#pragma  once
#include "asset_editor.h"

constexpr int MAX_SKINNED_MESHES = 64;

struct EditorSkinnedMesh
{
    const Name* asset_name;
    EditorMesh* mesh;
    int bone_index;
};

struct EditorBone
{
    const Name* name;
    int index;
    int parent_index;
    BoneTransform transform;
    Mat3 local_to_world;
    Mat3 world_to_local;
    float length;
    bool selected;
};

struct EditorSkeleton : EditorAsset
{
    int bone_count;
    EditorBone bones[MAX_BONES];
    EditorSkinnedMesh skinned_meshes[MAX_SKINNED_MESHES];
    int skinned_mesh_count;
    int selected_bone_count;
};

inline EditorSkeleton* GetEditorSkeleton(int index)
{
    EditorAsset* ea = GetEditorAsset(index);
    assert(ea->type == EDITOR_ASSET_TYPE_SKELETON);
    return (EditorSkeleton*)ea;
}

extern void InitEditorSkeleton(EditorAsset* ea);
extern EditorAsset* NewEditorSkeleton(const std::filesystem::path& path);
extern EditorAsset* CreateEditorSkeletonAsset(const std::filesystem::path& path, EditorSkeleton* skeleton);
extern void DrawEditorSkeleton(EditorSkeleton* es, bool selected);
extern void DrawEditorSkeleton(EditorSkeleton* es, const Vec2& position, bool selected);
extern void DrawEditorSkeletonBone(EditorSkeleton* es, int bone_index, const Vec2& position);
extern int HitTestBone(EditorSkeleton* es, const Vec2& world_pos);
extern void UpdateTransforms(EditorSkeleton* es);
extern void PostLoadEditorAssets(EditorSkeleton* es);
extern int FindBoneIndex(EditorSkeleton* es, const Name* name);
extern void Serialize(EditorSkeleton* es, Stream* stream);
extern Skeleton* ToSkeleton(Allocator* allocator, EditorSkeleton* es, const Name* name);
extern int ReparentBone(EditorSkeleton* es, int bone_index, int parent_index);
extern const Name* GetUniqueBoneName(EditorSkeleton* es);
extern void RemoveBone(EditorSkeleton* es, int bone_index);