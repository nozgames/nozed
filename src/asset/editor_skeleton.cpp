//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor/asset_editor.h"
#include "editor_asset.h"
#include "utils/file_helpers.h"

extern EditorAsset* CreateEditableAsset(const std::filesystem::path& path, EditableAssetType type);

void DrawEditorSkeleton(EditorAsset& ea)
{
    EditorSkeleton& skeleton = *ea.skeleton;
    DrawOrigin(ea.position);
    BindColor(ea.editing ? COLOR_SELECTED : COLOR_BLACK);
    for (int i=0; i<skeleton.bone_count; i++)
    {
        const EditorBone& bone = skeleton.bones[i];
        Vec2 bone_position = bone.transform * VEC2_ZERO;
        if (bone.parent_index < 0)
            continue;

        const EditorBone& ebone_parent = skeleton.bones[bone.parent_index];
        Vec2 parent_position = ebone_parent.transform * VEC2_ZERO;
        float len = Length(bone_position - parent_position);
        Vec2 dir = Normalize(bone_position - parent_position);
        BindTransform(TRS(ea.position + parent_position, dir, VEC2_ONE * len));
        DrawMesh(g_asset_editor.bone_mesh);
    }
}

EditorSkeleton* LoadEditorSkeleton(Allocator* allocator, const std::filesystem::path& path)
{
    std::string contents = ReadAllText(ALLOCATOR_DEFAULT, path);
    Tokenizer tk;
    Init(tk, contents.c_str());
    Token token={};

    EditorSkeleton* eskel = (EditorSkeleton*)Alloc(allocator, sizeof(EditorSkeleton));

    while (HasTokens(tk))
    {
        if (PeekChar(tk) == '\n')
        {
            NextChar(tk);
            SkipWhitespace(tk);
            continue;
        }

        if (!ExpectIdentifier(tk, &token))
            break;

        if (token.length != 1)
            break;

        if (token.value[0] == 'b')
        {
            if (!ExpectQuotedString(tk, &token))
                continue;

            const Name* bone_name = ToName(token);

            if (!ExpectQuotedString(tk, &token))
                continue;

            const Name* parent_name = ToName(token);

            EditorBone& bone = eskel->bones[eskel->bone_count++];
            bone.name = bone_name;
            bone.parent_name = parent_name;
            bone.leaf = true;

            SkipWhitespace(tk);

            while (PeekChar(tk) == 'p' || PeekChar(tk) == 'r')
            {
                char c = NextChar(tk);
                SkipWhitespace(tk);
                switch (c)
                {
                case 'p':
                {
                    float x;
                    if (!ExpectFloat(tk, &token, &x))
                        break;
                    float y;
                    if (!ExpectFloat(tk, &token, &y))
                        break;

                    bone.position.x = x;
                    bone.position.y = y;

                    SkipWhitespace(tk);

                    break;
                }

                case 'r':
                {
                    float r;
                    if (!ExpectFloat(tk, &token, &r))
                        break;

                    bone.rotation = r;
                    SkipWhitespace(tk);
                    break;
                }

                default:
                    break;
                }
            }
        }
    }

    for (int i=eskel->bone_count-1; i>=0; i--)
    {
        EditorBone& ebone1 = eskel->bones[i];
        ebone1.parent_index = -1;
        for (int j=i-1; j>=0; j--)
        {
            EditorBone& ebone2 = eskel->bones[j];
            if (ebone1.parent_name == ebone2.name)
            {
                ebone1.parent_index = j;
                ebone2.leaf = false;
                break;
            }
        }
    }

    for (int i=0; i<eskel->bone_count; i++)
    {
        EditorBone& ebone = eskel->bones[i];
        ebone.transform = TRS(ebone.position, ebone.rotation, VEC2_ONE);
        if (ebone.parent_index >= 0)
        {
            EditorBone& ebone_parent = eskel->bones[ebone.parent_index];
            ebone.transform = ebone.transform * ebone_parent.transform;
            ebone.length = Length(ebone.transform * VEC2_ZERO - ebone_parent.transform * VEC2_ZERO);
        }
        else
            ebone.length = 0.5f;
    }

    return eskel;
}

EditorAsset* CreateEditorSkeletonAsset(const std::filesystem::path& path)
{
    EditorSkeleton* skeleton = LoadEditorSkeleton(ALLOCATOR_DEFAULT, path);
    if (!skeleton)
        return nullptr;

    EditorAsset* ea = CreateEditableAsset(path, EDITABLE_ASSET_TYPE_SKELETON);
    ea->skeleton = skeleton;
    return ea;
}

EditorAsset* NewEditorSkeleton(const std::filesystem::path& path)
{
    const char* default_mesh = "b root p 0 0 r 0\n";

    std::filesystem::path full_path = path.is_relative() ?  std::filesystem::current_path() / "assets" / path : path;
    full_path += ".mesh";

    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);
    WriteCSTR(stream, default_mesh);
    SaveStream(stream, full_path);
    Free(stream);

    EditorAsset* ea = CreateEditorSkeletonAsset(full_path);
    if (!ea)
        return nullptr;

    g_asset_editor.assets[g_asset_editor.asset_count++] = ea;
    return ea;
}
