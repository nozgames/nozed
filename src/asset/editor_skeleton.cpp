//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor/asset_editor.h"
#include "editor_asset.h"
#include "utils/file_helpers.h"

void DrawEditorSkeleton(EditorAsset& ea, const Vec2& position, bool selected)
{
    EditorSkeleton& es = *ea.skeleton;

    BindColor(COLOR_WHITE);
    BindMaterial(g_asset_editor.material);
    for (int i=0; i<es.skinned_mesh_count; i++)
    {
        const EditorBone& bone = es.bones[es.skinned_meshes[i].bone_index];
        const EditorAsset& skinned_mesh_asset = *g_asset_editor.assets[es.skinned_meshes[i].asset_index];
        if (skinned_mesh_asset.type != EDITOR_ASSET_TYPE_MESH)
            continue;

        // Vec2 bone_position = bone.local_to_world * VEC2_ZERO;
        // Vec2 parent_position = es.bones[bone.parent_index >= 0 ? bone.parent_index : i].local_to_world * VEC2_ZERO;
        // Vec2 dir = Normalize(bone_position - parent_position);

        BindTransform(TRS(bone.local_to_world * VEC2_ZERO + position, 0, VEC2_ONE));
        DrawMesh(ToMesh(*skinned_mesh_asset.mesh));
    }

    BindMaterial(g_asset_editor.vertex_material);
    BindColor(selected ? COLOR_SELECTED : COLOR_BLACK);
    for (int i=1; i<es.bone_count; i++)
    {
        const EditorBone& bone = es.bones[i];
        Vec2 bone_position = bone.local_to_world * VEC2_ZERO;
        Vec2 parent_position = es.bones[bone.parent_index >= 0 ? bone.parent_index : i].local_to_world * VEC2_ZERO;
        Vec2 dir = Normalize(bone_position - parent_position);
        float len = Length(bone_position - parent_position);
        BindTransform(TRS(parent_position, dir, VEC2_ONE * len));
        DrawBone(parent_position + position, bone_position + position);
    }

    DrawOrigin(position);
}

void DrawEditorSkeleton(EditorAsset& ea, bool selected)
{
    DrawEditorSkeleton(ea, ea.position, selected);
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

static void ParseBonePosition(EditorBone& eb, Tokenizer& tk)
{
    float x;
    if (!ExpectFloat(tk, &x))
        ThrowError("misssing 'x' in bone position");
    float y;
    if (!ExpectFloat(tk, &y))
        ThrowError("misssing 'y' in bone position");

    eb.position.x = x;
    eb.position.y = y;
}

static void ParseBone(EditorSkeleton& es, Tokenizer& tk)
{
    if (!ExpectQuotedString(tk))
        ThrowError("expected bone name as quoted string");

    const Name* bone_name = GetName(tk);

    int parent_index = -1;
    if (!ExpectInt(tk, &parent_index))
        ThrowError("expected parent index");

    EditorBone& bone = es.bones[es.bone_count++];
    bone.name = bone_name;
    bone.parent_index = parent_index;

    while (!IsEOF(tk))
    {
        if (ExpectIdentifier(tk, "p"))
            ParseBonePosition(bone, tk);
        else
            break;
    }
}

EditorSkeleton* LoadEditorSkeleton(Allocator* allocator, const std::filesystem::path& path)
{
    std::string contents = ReadAllText(ALLOCATOR_DEFAULT, path);
    Tokenizer tk;
    Init(tk, contents.c_str());

    EditorSkeleton* es = (EditorSkeleton*)Alloc(allocator, sizeof(EditorSkeleton));

    while (!IsEOF(tk))
    {
        if (ExpectIdentifier(tk, "b"))
            ParseBone(*es, tk);
        else
            ThrowError("unknown identifier '%s' in skeleton", GetString(tk));
    }

    UpdateTransforms(*es);

    return es;
}

void SaveEditorSkeleton(const EditorSkeleton& es, const std::filesystem::path& path)
{
    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);

    for (int i=0; i<es.bone_count; i++)
    {
        const EditorBone& ev = es.bones[i];
        WriteCSTR(stream, "b \"%s\" %d p %g %g\n", ev.name->value, ev.parent_index, ev.position.x, ev.position.y);
    }

    SaveStream(stream, path);
    Free(stream);
}

EditorAsset* LoadEditorSkeletonAsset(const std::filesystem::path& path)
{
    EditorSkeleton* skeleton = LoadEditorSkeleton(ALLOCATOR_DEFAULT, path);
    if (!skeleton)
        return nullptr;

    EditorAsset* ea = CreateEditableAsset(path, EDITOR_ASSET_TYPE_SKELETON);
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

    EditorAsset* ea = LoadEditorSkeletonAsset(full_path);
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

void SaveAssetMetadata(const EditorSkeleton& es, Props* meta)
{
    for (int i=0; i<es.skinned_mesh_count; i++)
    {
        const EditorBone& bone = es.bones[i];
        meta->SetInt("skinned_meshes", es.skinned_meshes[i].asset_name->value, es.skinned_meshes[i].bone_index);
    }
}

void LoadAssetMetadata(EditorSkeleton& es, Props* meta)
{
    for (auto& key : meta->GetKeys("skinned_meshes"))
    {
        int bone_index = meta->GetInt("skinned_meshes", key.c_str(), -1);
        if (bone_index < 0 || bone_index >= es.bone_count)
            continue;

        es.skinned_meshes[es.skinned_mesh_count++] = {
            GetName(key.c_str()),
            -1,
            bone_index
        };
    }
}

void PostLoadEditorAssets(EditorSkeleton& es)
{
    for (int i=0; i<es.skinned_mesh_count; i++)
    {
        EditorSkinnedMesh& esm = es.skinned_meshes[i];
        esm.asset_index = FindEditorAssetByName(esm.asset_name);
        if (esm.asset_index < 0 || g_asset_editor.assets[esm.asset_index]->type != EDITOR_ASSET_TYPE_MESH)
            esm.asset_index = -1;
    }
}

int FindBoneIndex(const EditorSkeleton& es, const Name* name)
{
    for (int i=0; i<es.bone_count; i++)
        if (es.bones[i].name == name)
            return i;

    return -1;
}