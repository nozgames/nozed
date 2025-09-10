//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

// @STL

#pragma once

#include "asset_importer.h"
#include <filesystem>
#include <noz/noz.h>
#include <vector>

// Asset manifest generator function
// Scans the given output directory for asset files and generates a C manifest file
// containing asset metadata and memory requirements
//
// @param output_directory: Path to the directory containing imported assets
// @param manifest_output_path: Path where to generate the manifest C file
// @param importers: List of available importers to use for type lookups
// @return: true on success, false on failure
bool GenerateAssetManifest(
    const std::filesystem::path& output_directory, 
    const std::filesystem::path& manifest_output_path,
    const std::vector<AssetImporterTraits*>& importers,
    Props* config = nullptr);