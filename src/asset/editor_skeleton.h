//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#pragma  once

constexpr int MAX_BONES = 64;

struct EditorBone
{
    const Name* name;
    const Name* parent_name;
    Vec2 position;
    int parent_index;
    Mat3 local_to_world;
    Mat3 world_to_local;
    bool leaf;
    bool selected;
};

struct EditorSkeleton
{
    int bone_count;
    EditorBone bones[MAX_BONES];
    Bounds2 bounds;
};

extern EditorSkeleton* LoadEditorSkeleton(Allocator* allocator, const std::filesystem::path& path);
extern void DrawEditorSkeleton(EditorAsset& ea);
extern EditorAsset* NewEditorSkeleton(const std::filesystem::path& path);
extern EditorAsset* CreateEditorSkeletonAsset(const std::filesystem::path& path);
extern int HitTestBone(const EditorSkeleton& es, const Vec2& world_pos);
extern void UpdateTransforms(EditorSkeleton& es);