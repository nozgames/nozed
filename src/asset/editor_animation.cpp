//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#include <utils/file_helpers.h>
#include <utils/tokenizer.h>
#include <asset_editor/asset_editor.h>
#include "editor_asset.h"

void DrawEditorAnimation(EditorAsset& ea)
{
    EditorAnimation& en = *ea.anim;

    if (en.skeleton_asset == nullptr)
    {
        en.skeleton_asset = GetEditorAsset(FindEditorAssetByName(en.skeleton_name));
        UpdateBounds(en);
    }

    if (en.skeleton_asset)
        DrawEditorSkeleton(*en.skeleton_asset, ea.position, ea.selected && !ea.editing);
}

static void ParseSkeletonBone(EditorAnimation& en, Tokenizer& tk, const EditorSkeleton& es, int bone_index, int* bone_map)
{
    if (!ExpectQuotedString(tk))
        throw std::exception("missing quoted bone name");

    bone_map[bone_index] = FindBoneIndex(es, GetName(tk));
}

static void ParseSkeleton(EditorAnimation& en, Tokenizer& tk, int* bone_map)
{
    if (!ExpectQuotedString(tk))
        throw std::exception("missing quoted skeleton name");

    en.skeleton_name = GetName(tk);

    std::filesystem::path skeleton_path = GetEditorAssetPath(en.skeleton_name, ".skel");
    EditorSkeleton* es = LoadEditorSkeleton(ALLOCATOR_DEFAULT, skeleton_path);

    // Populate the bone list with the skeleton bones
    en.bone_count = es->bone_count;
    for (int i=0; i<es->bone_count; i++)
    {
        EditorAnimationBone& enb = en.bones[i];
        enb.name = es->bones[i].name;
        enb.index = i;
    }

    int bone_index = 0;
    while (!IsEOF(tk))
    {
        if (ExpectIdentifier(tk, "b"))
            ParseSkeletonBone(en, tk, *es, bone_index++, bone_map);
        else
            break;
    }
}

static int ParseFrameBone(EditorAnimation& ea, Tokenizer& tk, int* bone_map)
{
    int bone_index;
    if (!ExpectInt(tk, &bone_index))
        ThrowError("expected bone index");

    return bone_map[bone_index];
}

static void ParseFramePosition(EditorAnimation& ea, Tokenizer& tk, int bone_index, int frame_index)
{
    float x;
    if (!ExpectFloat(tk, &x))
        ThrowError("expected position 'x' value");
    float y;
    if (!ExpectFloat(tk, &y))
        ThrowError("expected position 'y' value");

    // Ignore the data since the bone is gone from the skeleton
    if (bone_index == -1)
        return;

    ea.bones[bone_index].position[frame_index] = {x,y};
}

static void ParseFrame(EditorAnimation& ea, Tokenizer& tk, int frame_index, int* bone_map)
{
    int bone_index = -1;
    while (!IsEOF(tk))
    {
        if (ExpectIdentifier(tk, "b"))
            bone_index = ParseFrameBone(ea, tk, bone_map);
        else if (ExpectIdentifier(tk, "p"))
            ParseFramePosition(ea, tk, bone_index, frame_index);
        else
            break;
    }
}

EditorAnimation* LoadEditorAnimation(Allocator* allocator, const std::filesystem::path& path)
{
    std::string contents = ReadAllText(ALLOCATOR_DEFAULT, path);
    Tokenizer tk;
    Init(tk, contents.c_str());

    EditorAnimation* en = (EditorAnimation*)Alloc(allocator, sizeof(EditorAnimation));
    int bone_map[MAX_BONES];
    for (int i=0; i<MAX_BONES; i++)
        bone_map[i] = -1;

    int frame_index = 0;

    try
    {
        while (!IsEOF(tk))
        {
            if (ExpectIdentifier(tk, "s"))
                ParseSkeleton(*en, tk, bone_map);
            if (ExpectIdentifier(tk, "f"))
                ParseFrame(*en, tk, frame_index++, bone_map);
            else
                ThrowError("invalid token '%s' in animation", GetString(tk));
        }
    }
    catch (std::exception& e)
    {
        Free(en);
        return nullptr;
    }

    en->bounds = { VEC2_NEGATIVE_ONE, VEC2_ONE };
    en->frame_count = frame_index;
    return en;
}

EditorAsset* LoadEditorAnimationAsset(const std::filesystem::path& path)
{
    EditorAnimation* en = LoadEditorAnimation(ALLOCATOR_DEFAULT, path);
    if (!en)
        return nullptr;

    EditorAsset* ea = CreateEditableAsset(path, EDITOR_ASSET_TYPE_ANIMATION);
    ea->anim = en;
    return ea;
}

void UpdateBounds(EditorAnimation& en)
{
    if (!en.skeleton_asset)
        return;

    en.bounds = en.skeleton_asset->skeleton->bounds;
}