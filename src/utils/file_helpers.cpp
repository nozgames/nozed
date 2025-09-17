
//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#include "file_helpers.h"

namespace fs = std::filesystem;

std::vector<fs::path> GetFilesInDirectory(const fs::path& directory)
{
    std::vector<fs::path> results;
    results.reserve(128);

    try
    {
        for (const auto& entry : fs::recursive_directory_iterator(directory))
            if (entry.is_regular_file())
                results.push_back(entry.path().string());
    }
    catch (const fs::filesystem_error&)
    {
    }

    return results;
}

static AssetSignature GetAssetSignatureInternal(Stream* stream)
{
    assert(stream);

    AssetHeader header = {};
    if (!ReadAssetHeader(stream, &header))
        return ASSET_SIGNATURE_UNKNOWN;

    return header.signature;
}

AssetSignature GetAssetSignature(const fs::path& path)
{
    Stream* stream = LoadStream(ALLOCATOR_DEFAULT, path);
    AssetSignature result = GetAssetSignatureInternal(stream);
    Free(stream);
    return result;
}

fs::path FixSlashes(const fs::path& path)
{
    std::string result = path.string();
    for (char& c : result)
        if (c == '\\')
            c = '/';
    return result;
}

std::string ReadAllText(Allocator* allocator, const fs::path& path)
{
    std::string result;

    Stream* stream = LoadStream(allocator, path);
    if (stream)
    {
        u32 size = GetSize(stream);
        if (size > 0)
        {
            result.resize(size + 1);
            ReadBytes(stream, result.data(), size);
            result[size] = 0;
        }
        Free(stream);
    }

    return result;
}

int CompareModifiedTime(const fs::file_time_type& a, const fs::file_time_type& b)
{
    if (a > b)
        return 1;

    if (a == b)
        return 0;

    return -1;
}

int CompareModifiedTime(const fs::path& a, const fs::path& b)
{
    auto a_time = fs::exists(a) ? fs::last_write_time(a) : fs::file_time_type {};
    auto b_time = fs::exists(b) ? fs::last_write_time(b) : fs::file_time_type {};
    if (a_time > b_time)
        return 1;

    if (a_time == b_time)
        return 0;

    return -1;
}

fs::path GetSafeFilename(const char* name)
{
    std::string result = name;
    Replace(result.data(), (u32)result.size(), ' ', '_');
    Replace(result.data(), (u32)result.size(), '-', '_');
    return std::move(result);
}
