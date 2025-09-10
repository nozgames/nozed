//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "../props.h"
#include "../../../src/vfx/vfx_internal.h"
#include "../tokenizer.h"

namespace fs = std::filesystem;

static bool ParseCurveType(Tokenizer& tk, VfxCurveType* curve_type)
{
    Token token;
    if (!ExpectIdentifier(tk, &token))
        return false;

    *curve_type = VFX_CURVE_TYPE_UNKNOWN;

    if (IsValue(token, "linear", true))
        *curve_type = VFX_CURVE_TYPE_LINEAR;
    else if (IsValue(token, "easein", true))
        *curve_type = VFX_CURVE_TYPE_EASE_IN;
    else if (IsValue(token, "easeout"))
        *curve_type = VFX_CURVE_TYPE_EASE_OUT;
    else if (IsValue(token, "easeinout"))
        *curve_type = VFX_CURVE_TYPE_EASE_IN_OUT;
    else if (IsValue(token, "quadratic"))
        *curve_type = VFX_CURVE_TYPE_QUADRATIC;
    else if (IsValue(token, "cubic"))
        *curve_type = VFX_CURVE_TYPE_CUBIC;
    else if (IsValue(token, "sine"))
        *curve_type = VFX_CURVE_TYPE_SINE;

    return *curve_type != VFX_CURVE_TYPE_UNKNOWN;
}

static bool ParseVec2(Tokenizer& tk, VfxVec2* value)
{
    Token token;

    SkipWhitespace(tk);

    if (!ExpectChar(tk,'['))
    {
        Vec2 s = {};
        if (!ExpectVec2(tk, &token, &s))
            return false;

        *value = {s, s};
        return true;
    }

    // Range
    Vec2 min = {0,0};
    if (!ExpectVec2(tk, &token, &min))
        return false;

    SkipWhitespace(tk);
    if (!ExpectChar(tk, ','))
        return false;

    Vec2 max = {0,0};
    if (!ExpectVec2(tk, &token, &max))
        return false;

    SkipWhitespace(tk);
    if (!ExpectChar(tk, ']'))
        return false;

    *value = { Min(min,max), Max(min,max) };
    return true;
}

static VfxVec2 ParseVec2(const std::string& str, const VfxVec2& default_value)
{
    if (str.empty())
        return default_value;

    Tokenizer tk;
    Init(tk, str.c_str());
    VfxVec2 value = {};
    if (!ParseVec2(tk, &value))
        return default_value;
    return value;
}

static bool ParseFloat(Tokenizer& tk, VfxFloat* value)
{
    Token token;

    SkipWhitespace(tk);

    // Single float?
    if (!ExpectChar(tk,'['))
    {
        f32 fvalue = 0.0f;
        if (!ExpectFloat(tk, &token, &fvalue))
            return false;

        *value = {fvalue, fvalue};
        return true;
    }

    // Range
    float min = 0.0f;
    if (!ExpectFloat(tk, &token, &min))
        return false;

    SkipWhitespace(tk);
    if (!ExpectChar(tk, ','))
        return false;

    float max = 0.0f;
    if (!ExpectFloat(tk, &token, &max))
        return false;

    SkipWhitespace(tk);
    if (!ExpectChar(tk, ']'))
        return false;

    *value = { Min(min, max), Max(min, max) };
    return true;
}

VfxFloat ParseFloat(const std::string& value, VfxFloat default_value)
{
    if (value.empty())
        return default_value;

    Tokenizer tk;
    Init(tk, value.c_str());
    ParseFloat(tk, &default_value);
    return default_value;
}

VfxFloatCurve ParseFloatCurve(const std::string& str, VfxFloatCurve default_value)
{
    Tokenizer tk;
    Init(tk, str.c_str());

    VfxFloatCurve value = { VFX_CURVE_TYPE_LINEAR };
    if (!ParseFloat(tk, &value.start))
        return default_value;

    SkipWhitespace(tk);
    if (!ExpectChar(tk, '-'))
    {
        value.end = value.start;
        return value;
    }

    if (!ExpectChar(tk, '>'))
        return default_value;

    SkipWhitespace(tk);

    if (!ParseFloat(tk, &value.end))
        return default_value;

    SkipWhitespace(tk);

    if (!ExpectChar(tk, ':'))
        return value;

    VfxCurveType curve_type = VFX_CURVE_TYPE_UNKNOWN;
    if (!ParseCurveType(tk, &curve_type))
        return default_value;

    value.type = curve_type;
    return value;
}

VfxInt ParseInt(const std::string& value, VfxInt default_value)
{
    if (value.empty())
        return default_value;

    Tokenizer tk;
    Init(tk, value.c_str());
    Token token;

    SkipWhitespace(tk);

    // Single int?
    if (!ExpectChar(tk,'['))
    {
        i32 ivalue = 0;
        if (!ExpectInt(tk, &token, &ivalue))
            return default_value;

        return { ivalue, ivalue };
    }

    // Range
    i32 min = 0;
    if (!ExpectInt(tk, &token, &min))
        return default_value;

    SkipWhitespace(tk);
    if (!ExpectChar(tk, ','))
        return default_value;

    i32 max = 0;
    if (!ExpectInt(tk, &token, &max))
        return default_value;

    SkipWhitespace(tk);
    if (!ExpectChar(tk, ']'))
        return default_value;

    return { Min(min,max), Max(min,max) };
}

static bool ParseColor(Tokenizer& tk, VfxColor* value)
{
    Token token;

    SkipWhitespace(tk);

    // Single float?
    if (!ExpectChar(tk,'['))
    {
        Color cvalue = {1.0f, 1.0f, 1.0f, 1.0f};
        if (!ExpectColor(tk, &token, &cvalue))
            return false;

        *value = {cvalue, cvalue};
        return true;
    }

    // Range
    Color min = {0,0,0,0};
    if (!ExpectColor(tk, &token, &min))
        return false;

    SkipWhitespace(tk);
    if (!ExpectChar(tk, ','))
        return false;

    Color max = {0,0,0,0};
    if (!ExpectColor(tk, &token, &max))
        return false;

    SkipWhitespace(tk);
    if (!ExpectChar(tk, ']'))
        return false;

    *value = { min, max };
    return true;
}

VfxColor ParseColor(const std::string& str, const VfxColor& default_value)
{
    if (str.empty())
        return default_value;

    Tokenizer tk;
    Init(tk, str.c_str());
    VfxColor value = {};
    if (!ParseColor(tk, &value))
        return default_value;
    return value;
}

VfxColorCurve ParseColorCurve(const std::string& str, const VfxColorCurve& default_value)
{
    Tokenizer tk;
    Init(tk, str.c_str());

    VfxColorCurve value = { VFX_CURVE_TYPE_LINEAR };
    if (!ParseColor(tk, &value.start))
        return default_value;

    SkipWhitespace(tk);
    if (!ExpectChar(tk, '-'))
    {
        value.end = value.start;
        return value;
    }

    if (!ExpectChar(tk, '>'))
        return default_value;

    SkipWhitespace(tk);

    if (!ParseColor(tk, &value.end))
        return default_value;

    SkipWhitespace(tk);

    if (!ExpectChar(tk, ':'))
        return value;

    VfxCurveType curve_type = VFX_CURVE_TYPE_UNKNOWN;
    if (!ParseCurveType(tk, &curve_type))
        return default_value;

    value.type = curve_type;
    return value;
}

void ImportVfx(const fs::path& source_path, Stream* output_stream, Props* config, Props* meta)
{
    Stream* input_stream = LoadStream(ALLOCATOR_DEFAULT, source_path);
    if (!input_stream)
        throw std::runtime_error("could not read file");

    Props* source = Props::Load(input_stream);
    if (!source)
    {
        Free(input_stream);
        throw std::runtime_error("could not load source file");
    }

    // Write asset header
    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE_VFX;
    header.version = 1;
    header.flags = 0;
    WriteAssetHeader(output_stream, &header);

    // Write header
    VfxFloat duration = ParseFloat(source->GetString("VFX", "duration", "5.0"), {5,5});
    bool loop = source->GetBool("vfx", "loop", false);
    WriteStruct(output_stream, duration);
    WriteBool(output_stream, loop);

    // Write emitters
    auto emitter_names = source->GetKeys("emitters");
    WriteU32(output_stream, emitter_names.size());
    for (const auto& emitter_name : emitter_names)
    {
        if (!source->HasGroup(emitter_name.c_str()))
            throw std::exception((std::string("missing emitter ") + emitter_name).c_str());

        std::string particle_section = emitter_name + ".particle";
        if (!source->HasGroup(particle_section.c_str()))
            throw std::exception((std::string("missing particle ") + particle_section).c_str());

        // Write emitter data
        WriteStruct(output_stream, ParseInt(source->GetString(emitter_name.c_str(), "rate", "0"), VFX_INT_ZERO));
        WriteStruct(output_stream, ParseInt(source->GetString(emitter_name.c_str(), "burst", "0"), VFX_INT_ZERO));
        WriteStruct(output_stream, ParseFloat(source->GetString(emitter_name.c_str(), "duration", "1.0"), VFX_FLOAT_ONE));
        WriteStruct(output_stream, ParseFloat(source->GetString(emitter_name.c_str(), "angle", "0..360"), {0.0f, 360.0f}));
        WriteStruct(output_stream, ParseFloat(source->GetString(emitter_name.c_str(), "radius", "0"), VFX_FLOAT_ZERO));
        WriteStruct(output_stream, ParseVec2(source->GetString(emitter_name.c_str(), "spawn", "(0, 0, 0)"), VFX_VEC2_ZERO));

        // Write particle data
        WriteString(output_stream, source->GetString(particle_section.c_str(), "mesh", "quad").c_str());
        WriteStruct(output_stream, ParseFloat(source->GetString(particle_section.c_str(), "duration", "1.0"), VFX_FLOAT_ONE));
        WriteStruct(output_stream, ParseFloatCurve(source->GetString(particle_section.c_str(), "size", "1.0"), VFX_FLOAT_CURVE_ONE));
        WriteStruct(output_stream, ParseFloatCurve(source->GetString(particle_section.c_str(), "speed", "0"), VFX_FLOAT_CURVE_ZERO));
        WriteStruct(output_stream, ParseColorCurve(source->GetString(particle_section.c_str(), "color", "white"), VFX_COLOR_CURVE_WHITE));
        WriteStruct(output_stream, ParseFloatCurve(source->GetString(particle_section.c_str(), "opacity", "1.0"), VFX_FLOAT_CURVE_ONE));
        WriteStruct(output_stream, ParseVec2(source->GetString(particle_section.c_str(), "gravity", "(0, 0, 0)"), VFX_VEC2_ZERO));
        WriteStruct(output_stream, ParseFloat(source->GetString(particle_section.c_str(), "drag", "0"), VFX_FLOAT_ZERO));
        WriteStruct(output_stream, ParseFloatCurve(source->GetString(particle_section.c_str(), "rotation", "0.0"), VFX_FLOAT_CURVE_ZERO));
    }

    Free(input_stream);
}

static const char* g_vfx_extensions[] = {
    ".vfx",
    nullptr
};

static AssetImporterTraits g_vfx_importer_traits = {
    .type_name = "Vfx",
    .signature = ASSET_SIGNATURE_VFX,
    .file_extensions = g_vfx_extensions,
    .import_func = ImportVfx
};

AssetImporterTraits* GetVfxImporterTraits()
{
    return &g_vfx_importer_traits;
}
