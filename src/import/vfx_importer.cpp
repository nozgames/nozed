//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

namespace fs = std::filesystem;

void ImportVfx(AssetData* a, const std::filesystem::path& path, Props* config, Props* meta) {
    (void)config;
    (void)meta;

    assert(a);
    assert(a->type == ASSET_TYPE_VFX);
    VfxData* evfx = (VfxData*)a;

    Stream* stream = CreateStream(nullptr, 4096);
    Serialize(evfx, stream);
    SaveStream(stream, path);
    Free(stream);
}

AssetImporter GetVfxImporter() {
    return {
        .type = ASSET_TYPE_VFX,
        .ext = ".vfx",
        .import_func = ImportVfx
    };
}
