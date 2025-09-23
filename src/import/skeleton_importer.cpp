//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

namespace fs = std::filesystem;

static void ImportSkeleton(EditorAsset* ea, Stream* output_stream, Props* config, Props* meta)
{
    (void)config;
    (void)meta;

    assert(ea);
    assert(ea->type == EDITOR_ASSET_TYPE_SKELETON);
    EditorSkeleton* es = (EditorSkeleton*)ea;
    Serialize(es, output_stream);
}

AssetImporter GetSkeletonImporter()
{
    return {
        .type = EDITOR_ASSET_TYPE_SKELETON,
        .signature = ASSET_SIGNATURE_SKELETON,
        .ext = ".skel",
        .import_func = ImportSkeleton
    };
}

