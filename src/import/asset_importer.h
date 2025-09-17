//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

#include <filesystem>

class Props;

struct AssetImporterTraits
{
    AssetSignature signature;
    const char* ext;
    void (*import_func) (const std::filesystem::path& source_path, Stream* output_stream, Props* config, Props* meta_props);
    bool (*does_depend_on) (const std::filesystem::path& source_path, const std::filesystem::path& dependency_path);
};
