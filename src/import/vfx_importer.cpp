//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include <utils/props.h>
#include <asset/editor_asset.h>

namespace fs = std::filesystem;

void ImportVfx(const fs::path& source_path, Stream* output_stream, Props* config, Props* meta)
{
    (config);
    (meta);
    EditorVfx* evfx = LoadEditorVfx(ALLOCATOR_DEFAULT, source_path);
    if (!evfx)
        throw std::exception("failed to load vfx");

    Serialize(*evfx, output_stream);
    Free(evfx);
}

static const char* g_vfx_extensions[] = {
    ".vfx",
    nullptr
};

static AssetImporterTraits g_vfx_importer_traits = {
    .type_name = "Vfx",
    .signature = ASSET_SIGNATURE_VFX,
    .file_extensions = g_vfx_extensions,
    .import_func = ImportVfx
};

AssetImporterTraits* GetVfxImporterTraits()
{
    return &g_vfx_importer_traits;
}
