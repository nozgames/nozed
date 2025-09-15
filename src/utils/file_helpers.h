//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once
#include <filesystem>

std::vector<std::filesystem::path> GetFilesInDirectory(const std::filesystem::path& directory);
AssetSignature GetAssetSignature(const std::filesystem::path& path);
std::filesystem::path FixSlashes(const std::filesystem::path& path);
std::string ReadAllText(Allocator* allocator, const std::filesystem::path& path);
int CompareModifiedTime(const std::filesystem::file_time_type& a, const std::filesystem::file_time_type& b);
int CompareModifiedTime(const std::filesystem::path& a, const std::filesystem::path& b);