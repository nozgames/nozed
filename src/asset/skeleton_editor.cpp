//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#include "../asset_editor.h"
#include "utils/file_helpers.h"
#include <editor.h>

extern Asset* LoadAssetInternal(Allocator* allocator, const Name* asset_name, AssetSignature signature, AssetLoaderFunc loader, Stream* stream);

void DrawEditorSkeleton(EditorAsset& ea, const Vec2& position, bool selected)
{
    EditorSkeleton& es = *ea.skeleton;

    UpdateTransforms(es);

    BindColor(COLOR_WHITE);
    BindMaterial(g_view.material);
    for (int i=0; i<es.skinned_mesh_count; i++)
    {
        EditorBone& bone = es.bones[es.skinned_meshes[i].bone_index];
        const EditorAsset& skinned_mesh_asset = *g_view.assets[es.skinned_meshes[i].asset_index];
        if (skinned_mesh_asset.type != EDITOR_ASSET_TYPE_MESH)
            continue;

        BindTransform(TRS(ea.position, 0, VEC2_ONE) * bone.local_to_world);
        DrawMesh(ToMesh(*skinned_mesh_asset.mesh));
    }

    BindMaterial(g_view.vertex_material);
    BindColor(selected ? COLOR_SELECTED : COLOR_BLACK);
    for (int i=1; i<es.bone_count; i++)
    {
        EditorBone& bone = es.bones[i];
        Vec2 bone_position = TransformPoint(bone.local_to_world);
        Vec2 parent_position = TransformPoint(es.bones[bone.parent_index >= 0 ? bone.parent_index : i].local_to_world);
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

int HitTestBone(EditorSkeleton& es, const Vec2& world_pos)
{
    const float size = g_view.select_size;
    float best_dist = F32_MAX;
    int best_bone_index = -1;
    for (int bone_index=0; bone_index<es.bone_count; bone_index++)
    {
        EditorBone& bone = es.bones[bone_index];
        Vec2 bone_position = TransformPoint(bone.local_to_world);
        float dist = Length(bone_position - world_pos);
        if (dist < size && dist < best_dist)
        {
            best_dist = dist;
            best_bone_index = bone_index;
        }
    }

    if (best_bone_index != -1)
        return best_bone_index;

    best_bone_index = -1;
    best_dist = F32_MAX;
    for (int bone_index=1; bone_index<es.bone_count; bone_index++)
    {
        EditorBone& bone = es.bones[bone_index];
        Vec2 b0 = TransformPoint(bone.local_to_world);
        Vec2 b1 = TransformPoint(es.bones[bone.parent_index].local_to_world);
        float dist = DistanceFromLine(b0, b1, world_pos);
        if (dist < size && dist < best_dist)
        {
            best_dist = dist;
            best_bone_index = bone_index;
        }
    }

    return best_bone_index;
}

static void ParseBonePosition(EditorBone& eb, Tokenizer& tk)
{
    float x;
    if (!ExpectFloat(tk, &x))
        ThrowError("misssing 'x' in bone position");
    float y;
    if (!ExpectFloat(tk, &y))
        ThrowError("misssing 'y' in bone position");

    eb.transform.position = {x,y};
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
    bone.transform.scale = VEC2_ONE;

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
        WriteCSTR(stream, "b \"%s\" %d p %g %g\n",
            ev.name->value,
            ev.parent_index,
            ev.transform.position.x,
            ev.transform.position.y);
    }

    SaveStream(stream, path);
    Free(stream);
}

EditorAsset* NewEditorSkeleton(const std::filesystem::path& path)
{
    const char* default_mesh = "b \"root\" -1 p 0 0\n";

    std::filesystem::path full_path = path.is_relative() ?  std::filesystem::current_path() / "assets" / path : path;
    full_path += ".skel";

    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);
    WriteCSTR(stream, default_mesh);
    SaveStream(stream, full_path);
    Free(stream);

    EditorAsset* ea = LoadEditorSkeletonAsset(full_path);
    if (!ea)
        return nullptr;

    g_view.assets[g_view.asset_count++] = ea;
    return ea;
}

void UpdateTransforms(EditorSkeleton& es)
{
    if (es.bone_count <= 0)
        return;

    EditorBone& root = es.bones[0];
    root.local_to_world = MAT3_IDENTITY;
    root.world_to_local = MAT3_IDENTITY;

    for (int bone_index=1; bone_index<es.bone_count; bone_index++)
    {
        EditorBone& bone = es.bones[bone_index];
        EditorBone& parent = es.bones[bone.parent_index];
        bone.local_to_world = parent.local_to_world * TRS(bone.transform.position, bone.transform.rotation, bone.transform.scale);
        bone.world_to_local = Inverse(root.local_to_world);
        bone.length = 1.0f; // Length(bone.position);
    }

    Vec2 root_position = TransformPoint(es.bones[0].local_to_world);
    Bounds2 bounds = Bounds2 { root_position, root_position };
    for (int i=1; i<es.bone_count; i++)
        bounds = Union(bounds, TransformPoint(es.bones[i].local_to_world));

    es.bounds = Expand(bounds, 0.5f);
}

void SaveAssetMetadata(const EditorSkeleton& es, Props* meta)
{
    for (int i=0; i<es.skinned_mesh_count; i++)
        meta->SetInt("skinned_meshes", es.skinned_meshes[i].asset_name->value, es.skinned_meshes[i].bone_index);
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

static void PostLoadEditorSkeleton(EditorAsset& ea)
{
    EditorSkeleton& es = *ea.skeleton;
    for (int i=0; i<es.skinned_mesh_count; i++)
    {
        EditorSkinnedMesh& esm = es.skinned_meshes[i];
        esm.asset_index = FindEditorAssetByName(esm.asset_name);
        if (esm.asset_index < 0 || g_view.assets[esm.asset_index]->type != EDITOR_ASSET_TYPE_MESH)
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

void Serialize(EditorSkeleton& es, Stream* output_stream)
{
    const Name* bone_names[MAX_BONES];
    for (int i=0; i<es.bone_count; i++)
        bone_names[i] = es.bones[i].name;

    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE_SKELETON;
    header.version = 1;
    header.flags = 0;
    header.names = es.bone_count;
    WriteAssetHeader(output_stream, &header, bone_names);

    WriteU8(output_stream, (u8)es.bone_count);

    for (int i=0; i<es.bone_count; i++)
    {
        EditorBone& eb = es.bones[i];
        WriteI8(output_stream, (char)eb.parent_index);
        WriteStruct(output_stream, eb.local_to_world);
        WriteStruct(output_stream, eb.world_to_local);
        WriteStruct(output_stream, eb.transform.position);
        WriteFloat(output_stream, eb.transform.rotation);
        WriteStruct(output_stream, eb.transform.scale);
    }
}

Skeleton* ToSkeleton(Allocator* allocator, EditorSkeleton& es, const Name* name)
{
    if (es.skeleton)
        return es.skeleton;

    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 8192);
    if (!stream)
        return nullptr;
    Serialize(es, stream);
    SeekBegin(stream, 0);

    Skeleton* skeleton = (Skeleton*)LoadAssetInternal(allocator, name, ASSET_SIGNATURE_SKELETON, LoadSkeleton, stream);
    Free(stream);

    es.skeleton = skeleton;

    return skeleton;
}

EditorAsset* LoadEditorSkeletonAsset(const std::filesystem::path& path)
{
    EditorSkeleton* skeleton = LoadEditorSkeleton(ALLOCATOR_DEFAULT, path);
    if (!skeleton)
        return nullptr;

    EditorAsset* ea = CreateEditorAsset(path, EDITOR_ASSET_TYPE_SKELETON);
    ea->skeleton = skeleton;
    ea->vtable = {
        .post_load = PostLoadEditorSkeleton
    };
    return ea;
}
