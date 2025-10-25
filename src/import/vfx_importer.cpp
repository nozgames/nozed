//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

namespace fs = std::filesystem;

void ImportVfx(AssetData* ea, Stream* output_stream, Props* config, Props* meta)
{
    (void)config;
    (void)meta;

    assert(ea);
    assert(ea->type == ASSET_TYPE_VFX);
    VfxData* evfx = (VfxData*)ea;
    Serialize(evfx, output_stream);
}

AssetImporter GetVfxImporter()
{
    return {
        .type = ASSET_TYPE_VFX,
        .signature = ASSET_SIGNATURE_VFX,
        .ext = ".vfx",
        .import_func = ImportVfx
    };
}
