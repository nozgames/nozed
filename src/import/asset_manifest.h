//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

// @STL

#pragma once

bool GenerateAssetManifest(
    const std::filesystem::path& source_path, 
    const std::filesystem::path& target_path,
    Props* config = nullptr);