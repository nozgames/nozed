//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once
#include <filesystem>

extern void GetFilesInDirectory(const std::filesystem::path& directory, std::vector<std::filesystem::path>& results);
extern AssetSignature GetAssetSignature(const std::filesystem::path& path);
extern std::filesystem::path FixSlashes(const std::filesystem::path& path);
extern std::string ReadAllText(Allocator* allocator, const std::filesystem::path& path);
extern int CompareModifiedTime(const std::filesystem::file_time_type& a, const std::filesystem::file_time_type& b);
extern int CompareModifiedTime(const std::filesystem::path& a, const std::filesystem::path& b);
extern std::filesystem::path GetSafeFilename(const char* name);
