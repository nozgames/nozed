//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#define STB_IMAGE_IMPLEMENTATION
#include "../external/stb_image.h"

namespace fs = std::filesystem;

static float SRGBToLinear(float srgb) {
    if (srgb <= 0.04045f)
        return srgb / 12.92f;

    return std::pow((srgb + 0.055f) / 1.055f, 2.4f);
}

static void ConvertSRGBToLinear(uint8_t* pixels, int width, int height, int channels) {
    int total_pixels = width * height;
    for (int i = 0; i < total_pixels; ++i) {
        for (int c = 0; c < Min(3, channels); ++c) {
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
    const std::string& clamp) {

    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE;
    header.type = ASSET_TYPE_TEXTURE;
    header.version = 1;
    header.flags = ASSET_FLAG_NONE;
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
    WriteBytes(stream, data, width * height * channels);
}

static void ImportTexture(AssetData* ea, Stream* output_stream, Props* config, Props* meta) {
    (void)config;

    fs::path src_path = ea->path;
    
    int width;
    int height;
    int channels;
    unsigned char* image_data = stbi_load(src_path.string().c_str(), &width, &height, &channels, 0);
    
    if (!image_data)
        throw std::runtime_error("Failed to load texture file");

    std::string filter = meta->GetString("texture", "filter", "linear");
    std::string clamp = meta->GetString("texture", "clamp", "clamp");
    bool convert_from_srgb = meta->GetBool("texture", "srgb", false);

    std::vector<uint8_t> rgba_data;
    if (channels != 4) {
        rgba_data.resize(width * height * 4);
        for (int i = 0; i < width * height; ++i) {
            for (int c = 0; c < 3; ++c)
                rgba_data[i * 4 + c] = (c < channels) ? image_data[i * channels + c] : 0;
            rgba_data[i * 4 + 3] = (channels == 4) ? image_data[i * channels + 3] : 255; // Alpha
        }
        channels = 4;
    } else {
        rgba_data.assign(image_data, image_data + (width * height * channels));
    }
    
    stbi_image_free(image_data);
    
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

AssetImporter GetTextureImporter() {
    return {
        .type = ASSET_TYPE_TEXTURE,
        .ext = ".png",
        .import_func = ImportTexture
    };
}
