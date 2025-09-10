//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#pragma  once

constexpr int MAX_BONES = 64;

struct EditorBone
{
    Vec2 position;
    float rotation;
    int parent_index;
};

struct EditorSkeleton
{
    int bone_count;
    EditorBone bones[MAX_BONES];
};

extern EditorSkeleton* LoadEditorSkeleton(Allocator* allocator, const std::filesystem::path& path);
