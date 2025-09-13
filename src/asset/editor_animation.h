//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

struct EditorAnimationBone
{
    const Name* name;
    int index;
    Transform frames[MAX_ANIMATION_FRAMES];
};

struct EditorAnimation
{
    const Name* skeleton_name;
    EditorAnimationBone bones[MAX_BONES];
    Mat3 bone_transforms[MAX_BONES];
    int bone_count;
    int frame_count;
    Bounds2 bounds;
    EditorAsset* skeleton_asset;
    int current_frame;
    Animation* animation;
};

extern EditorAnimation* LoadEditorAnimation(Allocator* allocator, const std::filesystem::path& path);
extern EditorAsset* LoadEditorAnimationAsset(const std::filesystem::path& path);
extern void DrawEditorAnimation(EditorAsset& ea);
extern void UpdateBounds(EditorAnimation& en);
extern void UpdateTransforms(EditorAnimation& en, int frame_index);
extern void Serialize(const EditorAnimation& en, Stream* output_stream);
extern Animation* ToAnimation(Allocator* allocator, EditorAnimation& en, const Name* name);
extern void SaveEditorAnimation(const EditorAnimation& en, const std::filesystem::path& path);
extern int InsertFrame(EditorAnimation& en, int frame_index);
extern int DeleteFrame(EditorAnimation& en, int frame_index);