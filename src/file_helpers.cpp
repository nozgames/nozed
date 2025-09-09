
//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#include "file_helpers.h"

std::vector<std::filesystem::path> GetFilesInDirectory(const std::filesystem::path& directory)
{
    std::vector<std::filesystem::path> results;
    results.reserve(128);

    try
    {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(directory))
            if (entry.is_regular_file())
                results.push_back(entry.path().string());
    }
    catch (const std::filesystem::filesystem_error& ex)
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

AssetSignature GetAssetSignature(const std::filesystem::path& path)
{
    PushScratch();
    Stream* stream = LoadStream(ALLOCATOR_SCRATCH, path);
    AssetSignature result = GetAssetSignatureInternal(stream);
    PopScratch();
    Free(stream);
    return result;
}

std::filesystem::path FixSlashes(const std::filesystem::path& path)
{
    std::string result = path.string();
    for (char& c : result)
        if (c == '\\')
            c = '/';
    return result;
}

std::string ReadAllText(const std::filesystem::path& path)
{
    std::string result;

    PushScratch();
    Stream* stream = LoadStream(ALLOCATOR_SCRATCH, path);
    if (stream)
    {
        size_t size = GetSize(stream);
        if (size > 0)
        {
            result.resize(size + 1);
            ReadBytes(stream, result.data(), size);
            result[size] = 0;
        }
        Free(stream);
    }
    PopScratch();

    return result;
}