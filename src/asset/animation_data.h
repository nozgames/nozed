//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

struct SkeletonData;
struct EventData;


struct AnimationBoneData {
    const Name* name;
    int index;
    bool selected;
    Transform saved_transform;
};

struct AnimationFrameData {
    Transform transforms[MAX_BONES];
    EventData* event;
    const Name* event_name;
    int hold;
};

struct RuntimeAnimationData {
    AnimationBoneData bones[MAX_BONES];
    AnimationFrameData frames[MAX_ANIMATION_FRAMES];
    Animator animator;
    Skin Skins[SKIN_MAX];
};

struct AnimationData : AssetData {
    const Name* skeleton_name;
    RuntimeAnimationData* data;
    AnimationBoneData* bones;
    AnimationFrameData* frames;
    SkeletonData* skeleton;
    Animation* animation;
    Animator* animator;
    Skin* skins;
    int frame_count;
    int current_frame;
    int bone_count;
    int selected_bone_count;
    int skin_count;
    AnimationFlags flags;
};

extern void InitAnimationData(AssetData* a);
extern AssetData* NewAnimationData(const std::filesystem::path& path);
extern void PostLoadEditorAssets(AnimationData* n);
extern void UpdateBounds(AnimationData* n);
extern void Serialize(AnimationData* n, Stream* output_stream, SkeletonData* s);
extern Animation* ToAnimation(Allocator* allocator, AnimationData* n);
extern int InsertFrame(AnimationData* n, int insert_at);
extern int DeleteFrame(AnimationData* n, int frame_index);
extern Transform& GetFrameTransform(AnimationData* n, int bone_index, int frame_index);
extern void UpdateTransforms(AnimationData* n, int frame_index=-1);
extern void UpdateSkeleton(AnimationData* n);
extern void DrawAnimationData(AssetData* a);
extern int HitTestBones(AnimationData* n, const Mat3& transform, const Vec2& position, int* bones, int max_bones=MAX_BONES);
extern int HitTestBone(AnimationData* n, const Mat3& transform, const Vec2& position);
extern int GetFrameCountWithHolds(AnimationData* n);
extern int GetFrameIndexWithHolds(AnimationData* n, int frame_index);