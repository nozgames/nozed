//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "../gltf.h"

namespace fs = std::filesystem;

static float QuaternionToRotation(const Vec4& q)
{
    return atan2f(2.0 * (q.w * q.z + q.x * q.y), 1.0 - 2.0 * (q.y * q.y + q.z * q.z));
}

void ImportSkeleton(const fs::path& source_path, Stream* output_stream, Props* config, Props* meta)
{
    const fs::path& src_path = source_path;

    GLTFLoader gltf;
    if (!gltf.open(src_path))
        throw std::runtime_error("Failed to open GLTF/GLB file");

    auto bones = gltf.ReadBones();
    gltf.Close();

    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE_SKELETON;
    header.version = 1;
    header.flags = 0;
    WriteAssetHeader(output_stream, &header);

    WriteU8(output_stream, (u8)bones.size());

    for (const auto& bone : bones)
    {
        Mat3 local_to_world = ToMat3(bone.local_to_world);
        Mat3 world_to_local = ToMat3(bone.world_to_local);

        WriteString(output_stream, bone.name.c_str());
        WriteI8(output_stream, (char)bone.index);
        WriteI8(output_stream, (char)bone.parent_index);
        WriteStruct(output_stream, local_to_world);
        WriteStruct(output_stream, world_to_local);
        WriteVec2(output_stream, Vec2{bone.position.x, bone.position.y});
        WriteFloat(output_stream, QuaternionToRotation(bone.rotation));
        WriteVec2(output_stream, Vec2{bone.scale.x, bone.scale.y});
        WriteFloat(output_stream, bone.length);
        WriteVec2(output_stream, Vec2{bone.direction.x, bone.direction.y});
    }
}

bool CanImportSkeleton(const fs::path& source_path)
{
    Stream* stream = LoadStream(ALLOCATOR_DEFAULT, fs::path(source_path.string() + ".meta"));
    if (!stream)
        return false;
    Props* props = Props::Load(stream);
    if (!props)
    {
        Free(stream);
        return false;
    }

    bool can_import = !props->GetBool("mesh", "skip_skeleton", true);
    Free(stream);
    delete props;
    return can_import;
}

static const char* g_skeleton_extensions[] = {
    ".glb",
    nullptr
};

static AssetImporterTraits g_skeleton_importer_traits = {
    .type_name = "Skeleton",
    .signature = ASSET_SIGNATURE_SKELETON,
    .file_extensions = g_skeleton_extensions,
    .import_func = ImportSkeleton,
    .can_import = CanImportSkeleton
};

AssetImporterTraits* GetSkeletonImporterTraits()
{
    return &g_skeleton_importer_traits;
}

