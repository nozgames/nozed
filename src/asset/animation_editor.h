//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

struct EditorAsset;

struct EditorAnimationBone
{
    const Name* name;
    int index;
};

struct EditorAnimation
{
    const Name* skeleton_name;
    int bone_count;
    int frame_count;
    int current_frame;
    EditorAnimationBone bones[MAX_BONES];
    Transform frames[MAX_BONES * MAX_ANIMATION_FRAMES];
    Bounds2 bounds;
    EditorAsset* skeleton_asset;
    Animation* animation;
    Animator animator;
};

extern EditorAnimation* LoadEditorAnimation(Allocator* allocator, const std::filesystem::path& path);
extern EditorAsset* LoadEditorAnimationAsset(const std::filesystem::path& path);
extern void PostLoadEditorAssets(EditorAnimation& en);
extern void UpdateBounds(EditorAnimation& en);
extern void Serialize(EditorAnimation& en, Stream* output_stream);
extern Animation* ToAnimation(Allocator* allocator, EditorAnimation& en, const Name* name);
extern void SaveEditorAnimation(EditorAnimation& en, const std::filesystem::path& path);
extern int InsertFrame(EditorAnimation& en, int frame_index);
extern int DeleteFrame(EditorAnimation& en, int frame_index);
extern Transform& GetFrameTransform(EditorAnimation& en, int bone_index, int frame_index);
extern bool HitTestBone(EditorAnimation& en, const Vec2& world_pos);