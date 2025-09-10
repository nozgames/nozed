//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once
#include <filesystem>

std::vector<std::filesystem::path> GetFilesInDirectory(const std::filesystem::path& directory);
AssetSignature GetAssetSignature(const std::filesystem::path& path);
std::filesystem::path FixSlashes(const std::filesystem::path& path);
std::string ReadAllText(Allocator* allocator, const std::filesystem::path& path);