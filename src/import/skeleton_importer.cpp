//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

namespace fs = std::filesystem;

void ImportSkeleton(const fs::path& source_path, Stream* output_stream, Props* config, Props* meta)
{
    (void)config;
    (void)meta;

    EditorSkeleton* es = LoadEditorSkeleton(source_path);
    if (!es)
        ThrowError("failed to load skeleton");

    Serialize(es, output_stream);
    Free(es);
}

static AssetImporterTraits g_skeleton_importer_traits = {
    .signature = ASSET_SIGNATURE_SKELETON,
    .ext = ".skel",
    .import_func = ImportSkeleton
};

AssetImporterTraits* GetSkeletonImporterTraits()
{
    return &g_skeleton_importer_traits;
}

