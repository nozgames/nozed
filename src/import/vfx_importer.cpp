//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

namespace fs = std::filesystem;

void ImportVfx(const fs::path& source_path, Stream* output_stream, Props* config, Props* meta)
{
    (void)config;
    (void)meta;

    EditorVfx* evfx = LoadEditorVfx(ALLOCATOR_DEFAULT, source_path);
    if (!evfx)
        throw std::exception("failed to load vfx");

    Serialize(*evfx, output_stream);
    Free(evfx);
}

static AssetImporterTraits g_vfx_importer_traits = {
    .signature = ASSET_SIGNATURE_VFX,
    .ext = ".vfx",
    .import_func = ImportVfx
};

AssetImporterTraits* GetVfxImporterTraits()
{
    return &g_vfx_importer_traits;
}
