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

    BindMaterial(g_asset_editor.vertex_material);
    BindColor(ea.selected && !ea.editing ? COLOR_SELECTED : COLOR_BLACK);
    for (int i=1; i<skeleton.bone_count; i++)
    {
        const EditorBone& bone = skeleton.bones[i];
        Vec2 bone_position = bone.local_to_world * VEC2_ZERO;
        Vec2 parent_position = skeleton.bones[bone.parent_index >= 0 ? bone.parent_index : i].local_to_world * VEC2_ZERO;
        Vec2 dir = Normalize(bone_position - parent_position);
        float len = Length(bone_position - parent_position);
        BindTransform(TRS(parent_position, dir, VEC2_ONE * len));
        DrawBone(parent_position + ea.position, bone_position + ea.position);
    }

    DrawOrigin(ea.position);
}

int HitTestBone(const EditorSkeleton& em, const Vec2& world_pos)
{
    const float size = g_asset_editor.select_size;
    for (int i=0; i<em.bone_count; i++)
    {
        const EditorBone& bone = em.bones[i];
        Vec2 bone_position = bone.local_to_world * VEC2_ZERO;
        if (Length(bone_position - world_pos) < size)
            return i;
    }

    return -1;
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

    UpdateTransforms(*eskel);

    return eskel;
}

void SaveEditorSkeleton(const EditorSkeleton& es, const std::filesystem::path& path)
{
    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);

    for (int i=0; i<es.bone_count; i++)
    {
        const EditorBone& ev = es.bones[i];
        WriteCSTR(stream, "b \"%s\" \"%s\" p %g %g\n", ev.name->value, ev.parent_name->value, ev.position.x, ev.position.y);
    }

    SaveStream(stream, path);
    Free(stream);
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

void UpdateTransforms(EditorSkeleton& es)
{
    if (es.bone_count <= 0)
        return;

    EditorBone& root = es.bones[0];
    root.local_to_world = TRS(root.position, 0, VEC2_ONE);
    root.world_to_local = Inverse(root.local_to_world);

    for (int i=1; i<es.bone_count; i++)
    {
        EditorBone& bone = es.bones[i];
        bone.local_to_world = TRS(bone.position, 0, VEC2_ONE) * es.bones[bone.parent_index].local_to_world;
        bone.world_to_local = Inverse(bone.local_to_world);
    }

    Vec2 root_position = es.bones[0].local_to_world * VEC2_ZERO;
    Bounds2 bounds = Bounds2 { root_position, root_position };
    for (int i=1; i<es.bone_count; i++)
    {
        Vec2 bone_position = es.bones[i].local_to_world * VEC2_ZERO;
        bounds = Union(bounds, bone_position);
    }

    es.bounds = Expand(bounds, 0.5f);
}
