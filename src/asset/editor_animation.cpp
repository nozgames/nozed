//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#include "editor_asset.h"
#include "../utils/file_helpers.h"
#include "../utils/tokenizer.h"

static void ParseSkeletonBone(EditorAnimation& en, Tokenizer& tk, const EditorSkeleton& es)
{
    if (!ExpectIdentifier(tk, "b"))
        throw std::exception("missing 'b'");

    if (!ExpectQuotedString(tk))
        throw std::exception("missing quoted bone name");

    EditorAnimationBone& bone = en.bones[en.bone_count++];
    bone.name = GetName(tk);
    //bone.index = GetBoneIndex(es, bone.name);
    if (bone.index < 0)
        en.bone_count--;
}

static void ParseSkeleton(EditorAnimation& en, Tokenizer& tk)
{
    if (!ExpectIdentifier(tk, "s"))
        throw std::exception("missing 's'");

    if (!ExpectQuotedString(tk))
        throw std::exception("missing quoted skeleton name");

    en.skeleton_name = GetName(tk);

    std::filesystem::path skeleton_path = GetEditorAssetPath(en.skeleton_name, ".skel");
    EditorSkeleton* es = LoadEditorSkeleton(ALLOCATOR_DEFAULT, skeleton_path);

    while (!IsEOF(tk))
    {
        if (!ExpectIdentifier(tk))
            ThrowError("expected identifier");

        if (Equals(tk, "b"))
            ParseSkeletonBone(en, tk, *es);
        else
            ThrowError("invalid token '%s' in skeleton", GetString(tk));
    }
}

EditorAnimation* LoadEditorAnimation(Allocator* allocator, const std::filesystem::path& path)
{
    std::string contents = ReadAllText(ALLOCATOR_DEFAULT, path);
    Tokenizer tk;
    Init(tk, contents.c_str());
    Token token={};

    EditorAnimation* en = (EditorAnimation*)Alloc(allocator, sizeof(EditorAnimation));

    int frame_index = 0;

    try
    {
        while (!IsEOF(tk))
        {
            if (!ExpectIdentifier(tk))
                ThrowError("expected identifier");

            if (Equals(tk, "s"))
                ParseSkeleton(*en, tk);
            else
                ThrowError("invalid token '%s' in animation", GetString(tk));
        }
    }
    catch (std::exception& e)
    {
        Free(en);
        return nullptr;
    }

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
