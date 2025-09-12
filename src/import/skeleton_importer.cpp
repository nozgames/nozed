//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

namespace fs = std::filesystem;

void ImportSkeleton(const fs::path& source_path, Stream* output_stream, Props* config, Props* meta)
{
#if 0
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
#endif
}

static const char* g_skeleton_extensions[] = {
    ".skel",
    nullptr
};

static AssetImporterTraits g_skeleton_importer_traits = {
    .type_name = "Skeleton",
    .signature = ASSET_SIGNATURE_SKELETON,
    .file_extensions = g_skeleton_extensions,
    .import_func = ImportSkeleton
};

AssetImporterTraits* GetSkeletonImporterTraits()
{
    return &g_skeleton_importer_traits;
}

