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
    float rotation;
    int parent_index;
    float length;
    Mat3 transform;
    bool leaf;
};

struct EditorSkeleton
{
    int bone_count;
    EditorBone bones[MAX_BONES];
};

extern EditorSkeleton* LoadEditorSkeleton(Allocator* allocator, const std::filesystem::path& path);
extern void DrawEditorSkeleton(EditorSkeleton& skeleton, const Vec2& pos);
extern EditorAsset* NewEditorSkeleton(const std::filesystem::path& path);
extern EditorAsset* CreateEditorSkeletonAsset(const std::filesystem::path& path);