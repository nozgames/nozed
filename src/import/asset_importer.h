//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

#include "../utils/props.h"
#include <filesystem>

struct AssetImporterTraits
{
    const char* type_name;           // e.g. "shader", "texture", "mesh"
    AssetSignature signature;     // e.g. ASSET_SIGNATURE_SHADER, ASSET_SIGNATURE_TEXTURE
    const char** file_extensions;    // NULL-terminated array of supported extensions (e.g. {".png", ".jpg", NULL})
    void (*import_func) (const std::filesystem::path& source_path, Stream* output_stream, Props* config, Props* meta_props);
    bool (*does_depend_on) (const std::filesystem::path& source_path, const std::filesystem::path& dependency_path);
    bool (*can_import) (const std::filesystem::path& source_path);
};
