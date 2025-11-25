//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "../msdf/msdf.h"
#include "../msdf/shape.h"
#include "../ttf/TrueTypeFont.h"
#include "../utils/rect_packer.h"
#include <filesystem>

namespace fs = std::filesystem;

using namespace noz;

struct ImportFontGlyph
{
    const ttf::TrueTypeFont::Glyph* ttf;
    Vec2Double size;
    Vec2Double scale;
    Vec2Double advance;
    Vec2Double bearing;
    Vec2Int packed_size;
    rect_packer::BinRect packed_rect;
    char ascii;
};

static void WriteFontData(
    Stream* stream,
    const ttf::TrueTypeFont* ttf,
    const std::vector<unsigned char>& atlas_data,
    const Vec2Int& atlas_size,
    const std::vector<ImportFontGlyph>& glyphs,
    int font_size)
{
    float font_size_inv = 1.0f / (f32)font_size;

    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE;
    header.type = ASSET_TYPE_FONT;
    header.version = 1;
    header.flags = 0;
    WriteAssetHeader(stream, &header);

    WriteU32(stream, static_cast<u32>(font_size));
    WriteU32(stream, static_cast<u32>(atlas_size.x));
    WriteU32(stream, static_cast<u32>(atlas_size.y));
    WriteFloat(stream, (f32)ttf->ascent() * font_size_inv);
    WriteFloat(stream, (f32)ttf->descent() * font_size_inv);
    WriteFloat(stream, (f32)(ttf->height() + ttf->descent()) * font_size_inv);
    WriteFloat(stream, 0.0f);

    // Write glyph count and glyph data
    WriteU16(stream, static_cast<uint16_t>(glyphs.size()));
    for (const auto& glyph : glyphs)
    {
        WriteU32(stream, glyph.ascii);
        WriteFloat(stream, (f32)glyph.packed_rect.x / (f32)atlas_size.x);
        WriteFloat(stream, (f32)glyph.packed_rect.y / (f32)atlas_size.y);
        WriteFloat(stream, (f32)(glyph.packed_rect.x + glyph.packed_rect.w) / (f32)atlas_size.x);
        WriteFloat(stream, (f32)(glyph.packed_rect.y + glyph.packed_rect.h) / (f32)atlas_size.y);
        WriteFloat(stream, (f32)glyph.size.x * font_size_inv);
        WriteFloat(stream, (f32)glyph.size.y * font_size_inv);
        WriteFloat(stream, (f32)glyph.advance.x * font_size_inv);
        WriteFloat(stream, (f32)glyph.bearing.x * font_size_inv);
        WriteFloat(stream, (f32)(glyph.ttf->size.y - glyph.ttf->bearing.y) * font_size_inv);
    }

    // Write kerning count and kerning data
    WriteU16(stream, static_cast<uint16_t>(ttf->kerning().size()));
    for (const auto& k : ttf->kerning())
    {
        WriteU32(stream, k.left);
        WriteU32(stream, k.right);
        WriteFloat(stream, k.value);
    }

    WriteBytes(stream, atlas_data.data(), (u32)atlas_data.size());
}

static void ImportFont(AssetData* ea, Stream* output_stream, Props* config, Props* meta)
{
    (void)config;

    // Parse font properties from meta props (with defaults)
    int font_size = meta->GetInt("font", "size", 48);
    std::string characters = meta->GetString("font", "characters", " !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~");
    float sdf_range = meta->GetFloat("sdf", "range", 8);
    int padding = meta->GetInt("font", "padding", 1);

    // Load font file
    std::ifstream file(ea->path, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("Failed to open font file");

    // Get file size
    file.seekg(0, std::ios::end);
    size_t file_size = file.tellg();
    file.seekg(0, std::ios::beg);

    // Read font data
    std::vector<unsigned char> fontData(file_size);
    file.read(reinterpret_cast<char*>(fontData.data()), file_size);
    file.close();

    // Create stream for font loading
    Stream* stream = LoadStream(nullptr, fontData.data(), (u32)fontData.size());
    auto ttf = std::shared_ptr<ttf::TrueTypeFont>(ttf::TrueTypeFont::load(stream, font_size, characters));

    // Build the imported glyph list
    std::vector<ImportFontGlyph> glyphs;
    for (size_t i = 0; i < characters.size(); i++)
    {
        auto ttf_glyph = ttf->glyph(characters[i]);
        if (ttf_glyph == nullptr)
            continue;

        ImportFontGlyph iglyph{};
        iglyph.ascii = characters[i];
        iglyph.ttf = ttf_glyph;
        iglyph.size = ttf_glyph->size + Vec2Double{sdf_range * 2, sdf_range * 2};
        iglyph.scale = {1,1};
        iglyph.packed_size = RoundToNearest(iglyph.size + Vec2Double{padding * 2.0, padding * 2.0});
        iglyph.bearing = ttf_glyph->bearing - Vec2Double{sdf_range, sdf_range};
        iglyph.advance.x = (f32)ttf_glyph->advance;

        glyphs.push_back(iglyph);
    }

    // Pack the glyphs
    int minHeight = (int)NextPowerOf2((u32)(font_size + 2 + sdf_range * 2 + padding * 2));
    rect_packer packer(minHeight, minHeight);

    while (packer.empty())
    {
        for(auto& glyph : glyphs)
        {
            if (glyph.ttf->contours.size() == 0)
                continue;

            if (-1 == packer.Insert(glyph.packed_size, rect_packer::method::BestLongSideFit, glyph.packed_rect))
            {
                rect_packer::BinSize size = packer.size();
                if (size.w <= size.h)
                    size.w <<= 1;
                else
                    size.h <<= 1;

                packer.Resize(size.w, size.h);
                break;
            }
        }
    }

    if (!packer.validate())
        throw std::runtime_error("RectPacker validation failed");

    auto imageSize = Vec2Int(packer.size().w, packer.size().h);
    std::vector<uint8_t> image;
    image.resize(imageSize.x * imageSize.y, 0);

    for (size_t i = 0; i < glyphs.size(); i++)
    {
        auto glyph = glyphs[i];
        if (glyph.ttf->contours.size() == 0)
            continue;

        msdf::Shape* shape = msdf::Shape::fromGlyph(glyph.ttf, true);
        if (!shape)
            continue;

        msdf::renderGlyph(
            glyph.ttf,
            image,
            imageSize.x,
            {
                glyph.packed_rect.x + padding,
                glyph.packed_rect.y + padding
            },
            {
                glyph.packed_rect.w - padding * 2,
                glyph.packed_rect.h - padding * 2
            },
            sdf_range * 0.5f,
            {1,1},
            {
                -glyph.ttf->bearing.x + sdf_range,
                glyph.ttf->size.y - glyph.ttf->bearing.y + sdf_range
            }
        );

        delete shape;
    }

    WriteFontData(output_stream, ttf.get(), image, imageSize, glyphs, font_size);
}

AssetImporter GetFontImporter()
{
    return {
        .type = ASSET_TYPE_FONT,
        .ext = ".ttf",
        .import_func = ImportFont
    };
}
