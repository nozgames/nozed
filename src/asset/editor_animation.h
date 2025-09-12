//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

struct EditorAnimationBone
{
    const Name* name;
    int index;
    Vec2 position[MAX_ANIMATION_FRAMES];
};

struct EditorAnimation
{
    const Name* skeleton_name;
    EditorAnimationBone bones[MAX_BONES];
    int bone_count;
    int frame_count;
    Bounds2 bounds;
    EditorAsset* skeleton_asset;
};

extern EditorAnimation* LoadEditorAnimation(Allocator* allocator, const std::filesystem::path& path);
extern EditorAsset* LoadEditorAnimationAsset(const std::filesystem::path& path);
extern void DrawEditorAnimation(EditorAsset& ea);

