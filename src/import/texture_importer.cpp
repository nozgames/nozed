//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#define STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"

#include <string>
#include <filesystem>

namespace fs = std::filesystem;


static float SRGBToLinear(float srgb)
{
    if (srgb <= 0.04045f)
        return srgb / 12.92f;

    return std::pow((srgb + 0.055f) / 1.055f, 2.4f);
}

static void ConvertSRGBToLinear(uint8_t* pixels, int width, int height, int channels)
{
    int total_pixels = width * height;
    for (int i = 0; i < total_pixels; ++i)
    {
        // Convert RGB channels (skip alpha)
        for (int c = 0; c < Min(3, channels); ++c)
        {
            uint8_t srgb_value = pixels[i * channels + c];
            float srgb_float = (float)srgb_value / 255.0f;
            float linear_float = SRGBToLinear(srgb_float);
            pixels[i * channels + c] = static_cast<uint8_t>(std::round(linear_float * 255.0f));
        }
    }
}

static void WriteTextureData(
    Stream* stream,
    const uint8_t* data,
    int width,
    int height,
    int channels,
    const std::string& filter,
    const std::string& clamp)
{
    // Write asset header
    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE_TEXTURE;
    header.version = 1;
    header.flags = 0;
    WriteAssetHeader(stream, &header);
    
    TextureFilter filter_value = filter == "nearest" || filter == "point"
        ? TEXTURE_FILTER_NEAREST
        : TEXTURE_FILTER_LINEAR;

    TextureClamp clamp_value = clamp == "repeat" ?
        TEXTURE_CLAMP_REPEAT :
        TEXTURE_CLAMP_CLAMP;
    
    TextureFormat format = TEXTURE_FORMAT_RGBA8;
    WriteU8(stream, (u8)format);
    WriteU8(stream, (u8)filter_value);
    WriteU8(stream, (u8)clamp_value);
    WriteU32(stream, width);
    WriteU32(stream, height);

    // Write pixel data
    u32 data_size = width * height * channels;
    WriteBytes(stream, data, data_size);
}

void ImportTexture(const fs::path& source_path, Stream* output_stream, Props* config, Props* meta)
{
    (void)config;

    fs::path src_path = source_path;
    
    // Load image using stb_image
    int width, height, channels;
    unsigned char* image_data = stbi_load(src_path.string().c_str(), &width, &height, &channels, 0);
    
    if (!image_data)
    {
        throw std::runtime_error("Failed to load texture file");
    }
    
    std::string filter = meta->GetString("texture", "filter", "linear");
    std::string clamp = meta->GetString("texture", "clamp", "clamp");
    bool convert_from_srgb = meta->GetBool("texture", "srgb", false);

    // Convert to RGBA if needed
    std::vector<uint8_t> rgba_data;
    if (channels != 4)
    {
        rgba_data.resize(width * height * 4);
        for (int i = 0; i < width * height; ++i)
        {
            for (int c = 0; c < 3; ++c)
            {
                rgba_data[i * 4 + c] = (c < channels) ? image_data[i * channels + c] : 0;
            }
            rgba_data[i * 4 + 3] = (channels == 4) ? image_data[i * channels + 3] : 255; // Alpha
        }
        channels = 4;
    }
    else
    {
        rgba_data.assign(image_data, image_data + (width * height * channels));
    }
    
    stbi_image_free(image_data);
    
    // Convert from sRGB to linear if requested
    if (convert_from_srgb)
        ConvertSRGBToLinear(rgba_data.data(), width, height, channels);

    WriteTextureData(
        output_stream,
        rgba_data.data(),
        width,
        height,
        channels,
        filter,
        clamp
    );
}

static AssetImporterTraits g_texture_importer_traits = {
    .type_name = "Texture",
    .signature = ASSET_SIGNATURE_TEXTURE,
    .ext = ".png",
    .import_func = ImportTexture
};

AssetImporterTraits* GetTextureImporterTraits()
{
    return &g_texture_importer_traits;
}
