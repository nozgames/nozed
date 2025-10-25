//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

struct SkeletonData;

struct AnimationBoneData {
    const Name* name;
    int index;
    bool selected;
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
};

extern void InitAnimationData(AssetData* ea);
extern AssetData* NewAnimationData(const std::filesystem::path& path);
extern void PostLoadEditorAssets(AnimationData* en);
extern void UpdateBounds(AnimationData* en);
extern void Serialize(AnimationData* en, Stream* output_stream, SkeletonData* es);
extern Animation* ToAnimation(Allocator* allocator, AnimationData* en, const Name* name);
extern int InsertFrame(AnimationData* en, int frame_index);
extern int DeleteFrame(AnimationData* en, int frame_index);
extern Transform& GetFrameTransform(AnimationData* en, int bone_index, int frame_index);
extern int HitTestBone(AnimationData* en, const Vec2& world_pos);
extern void UpdateTransforms(AnimationData* en);
extern void UpdateSkeleton(AnimationData* en);
extern void DrawAnimationData(AssetData* ea);
extern void DrawEditorAnimationBone(AnimationData* en, int bone_index, const Vec2& position);
