//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include <asset/editor_asset.h>

namespace fs = std::filesystem;

void ImportSkeleton(const fs::path& source_path, Stream* output_stream, Props* config, Props* meta)
{
    (void)config;
    (void)meta;

    EditorSkeleton* es = LoadEditorSkeleton(ALLOCATOR_DEFAULT, source_path);
    if (!es)
        ThrowError("failed to load skeleton");

    Serialize(*es, output_stream);
    Free(es);
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

