//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

namespace fs = std::filesystem;

static void ImportSkeleton(AssetData* a, const std::filesystem::path& path, Props* config, Props* meta) {
    (void)config;
    (void)meta;

    assert(a);
    assert(a->type == ASSET_TYPE_SKELETON);
    SkeletonData* s = (SkeletonData*)a;

    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);
    Serialize(s, stream);
    SaveStream(stream, path);
    Free(stream);
}

AssetImporter GetSkeletonImporter()
{
    return {
        .type = ASSET_TYPE_SKELETON,
        .ext = ".skel",
        .import_func = ImportSkeleton
    };
}

