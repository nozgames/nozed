//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

struct EditorSkeleton;

struct EditorAnimationBone
{
    const Name* name;
    int index;
    bool selected;
};

struct EditorAnimation : EditorAsset
{
    const Name* skeleton_name;
    int frame_count;
    int current_frame;
    EditorAnimationBone bones[MAX_BONES];
    int bone_count;
    Transform frames[MAX_BONES * MAX_ANIMATION_FRAMES];
    EditorSkeleton* skeleton;
    Animation* animation;
    Animator animator;
    int selected_bone_count;
};

extern void InitEditorAnimation(EditorAsset* ea);
extern EditorAsset* NewEditorAnimation(const std::filesystem::path& path);
extern void PostLoadEditorAssets(EditorAnimation* en);
extern void UpdateBounds(EditorAnimation* en);
extern void Serialize(EditorAnimation* en, Stream* output_stream, EditorSkeleton* es);
extern Animation* ToAnimation(Allocator* allocator, EditorAnimation* en, const Name* name);
extern int InsertFrame(EditorAnimation* en, int frame_index);
extern int DeleteFrame(EditorAnimation* en, int frame_index);
extern Transform& GetFrameTransform(EditorAnimation* en, int bone_index, int frame_index);
extern int HitTestBone(EditorAnimation* en, const Vec2& world_pos);
extern void UpdateTransforms(EditorAnimation* en);
extern void UpdateSkeleton(EditorAnimation* en);
extern void DrawEditorAnimation(EditorAsset* ea);
extern void DrawEditorAnimationBone(EditorAnimation* en, int bone_index, const Vec2& position);
