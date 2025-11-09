//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

struct SkeletonData;

struct AnimationBoneData {
    const Name* name;
    int index;
    bool selected;
    Transform saved_transform;
};

struct AnimationFrameData {
    Transform transforms[MAX_BONES];
    int hold;
};

struct AnimationData : AssetData {
    const Name* skeleton_name;
    int frame_count;
    int current_frame;
    AnimationBoneData bones[MAX_BONES];
    int bone_count;
    AnimationFrameData frames[MAX_ANIMATION_FRAMES];
    SkeletonData* skeleton;
    Animation* animation;
    Animator animator;
    int selected_bone_count;
    AnimationFlags flags;
};

extern void InitAnimationData(AssetData* a);
extern AssetData* NewAnimationData(const std::filesystem::path& path);
extern void PostLoadEditorAssets(AnimationData* n);
extern void UpdateBounds(AnimationData* n);
extern void Serialize(AnimationData* n, Stream* output_stream, SkeletonData* s);
extern Animation* ToAnimation(Allocator* allocator, AnimationData* n);
extern int InsertFrame(AnimationData* n, int frame_index);
extern int DeleteFrame(AnimationData* n, int frame_index);
extern Transform& GetFrameTransform(AnimationData* n, int bone_index, int frame_index);
extern int HitTestBone(AnimationData* n, const Vec2& world_pos);
extern void UpdateTransforms(AnimationData* n);
extern void UpdateSkeleton(AnimationData* n);
extern void DrawAnimationData(AssetData* a);
extern void DrawEditorAnimationBone(AnimationData* n, int bone_index, const Vec2& position);
