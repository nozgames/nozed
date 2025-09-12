//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include <asset/editor_asset.h>

namespace fs = std::filesystem;

void ImportSkeleton(const fs::path& source_path, Stream* output_stream, Props* config, Props* meta)
{
    EditorSkeleton* es = LoadEditorSkeleton(ALLOCATOR_DEFAULT, source_path);
    if (!es)
        ThrowError("failed to load skeleton");

    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE_SKELETON;
    header.version = 1;
    header.flags = 0;
    WriteAssetHeader(output_stream, &header);

    WriteU8(output_stream, (u8)es->bone_count);

    for (int i=0; i<es->bone_count; i++)
    {
        EditorBone& eb = es->bones[i];
        WriteString(output_stream, eb.name->value);
        WriteI8(output_stream, (char)eb.parent_index);
        WriteStruct(output_stream, eb.local_to_world);
        WriteStruct(output_stream, eb.world_to_local);
    }
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

