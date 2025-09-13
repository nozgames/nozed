//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#include "editor_asset.h"
#include <view.h>
#include <utils/file_helpers.h>
#include "../../../src/vfx/vfx_internal.h"

extern Asset* LoadAssetInternal(Allocator* allocator, const Name* asset_name, AssetSignature signature, AssetLoaderFunc loader, Stream* stream);
extern EditorAsset* CreateEditableAsset(const std::filesystem::path& path, EditorAssetType type);

void DrawEditorVfx(EditorAsset& ea)
{
    if (!IsPlaying(ea.vfx_handle) && ea.vfx->vfx)
        ea.vfx_handle = Play(ea.vfx->vfx, ea.position);

    DrawOrigin(ea);
}

static bool ParseCurveType(Tokenizer& tk, VfxCurveType* curve_type)
{
    if (!ExpectIdentifier(tk))
        return false;

    *curve_type = VFX_CURVE_TYPE_UNKNOWN;

    if (Equals(tk, "linear", true))
        *curve_type = VFX_CURVE_TYPE_LINEAR;
    else if (Equals(tk, "easein", true))
        *curve_type = VFX_CURVE_TYPE_EASE_IN;
    else if (Equals(tk, "easeout"))
        *curve_type = VFX_CURVE_TYPE_EASE_OUT;
    else if (Equals(tk, "easeinout"))
        *curve_type = VFX_CURVE_TYPE_EASE_IN_OUT;
    else if (Equals(tk, "quadratic"))
        *curve_type = VFX_CURVE_TYPE_QUADRATIC;
    else if (Equals(tk, "cubic"))
        *curve_type = VFX_CURVE_TYPE_CUBIC;
    else if (Equals(tk, "sine"))
        *curve_type = VFX_CURVE_TYPE_SINE;

    return *curve_type != VFX_CURVE_TYPE_UNKNOWN;
}

static bool ParseVec2(Tokenizer& tk, VfxVec2* value)
{
    // Non range
    if (!ExpectDelimiter(tk, '['))
    {
        Vec2 v;
        if (!ExpectVec2(tk, &v))
            return false;

        *value = {v, v};
        return true;
    }

    // Range
    Vec2 min = {0,0};
    if (!ExpectVec2(tk, &min))
        return false;

    if (!ExpectDelimiter(tk, ','))
        return false;

    Vec2 max = {0,0};
    if (!ExpectVec2(tk, &max))
        return false;

    if (!ExpectDelimiter(tk, ']'))
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
    // Non range
    if (!ExpectDelimiter(tk, '['))
    {
        float v;
        if (!ExpectFloat(tk, &v))
            return false;

        *value = {v, v};
        return true;
    }

    // Range
    float min = 0.0f;
    if (!ExpectFloat(tk, &min))
        return false;

    if (!ExpectDelimiter(tk, ','))
        return false;

    float max = 0.0f;
    if (!ExpectFloat(tk, &max))
        return false;

    if (!ExpectDelimiter(tk, ']'))
        return false;

    *value = { Min(min,max), Max(min,max) };
    return true;
}

static VfxFloat ParseFloat(const std::string& value, VfxFloat default_value)
{
    if (value.empty())
        return default_value;

    Tokenizer tk;
    Init(tk, value.c_str());
    ParseFloat(tk, &default_value);
    return default_value;
}

static VfxFloatCurve ParseFloatCurve(const std::string& str, VfxFloatCurve default_value)
{
    Tokenizer tk;
    Init(tk, str.c_str());

    VfxFloatCurve value = { VFX_CURVE_TYPE_LINEAR };
    if (!ParseFloat(tk, &value.start))
        return default_value;

    if (!ExpectDelimiter(tk, '='))
    {
        value.end = value.start;
        return value;
    }

    if (!ExpectDelimiter(tk, '>'))
        return default_value;

    if (!ParseFloat(tk, &value.end))
        return default_value;

    if (!ExpectDelimiter(tk, ':'))
        return value;

    VfxCurveType curve_type = VFX_CURVE_TYPE_UNKNOWN;
    if (!ParseCurveType(tk, &curve_type))
        return default_value;

    value.type = curve_type;
    return value;
}

static VfxInt ParseInt(const std::string& value, VfxInt default_value)
{
    if (value.empty())
        return default_value;

    Tokenizer tk;
    Init(tk, value.c_str());

    // Single int?
    if (!ExpectDelimiter(tk,'['))
    {
        i32 ivalue = 0;
        if (!ExpectInt(tk, &ivalue))
            return default_value;

        return { ivalue, ivalue };
    }

    // Range
    i32 min = 0;
    if (!ExpectInt(tk, &min))
        return default_value;

    if (!ExpectDelimiter(tk, ','))
        return default_value;

    i32 max = 0;
    if (!ExpectInt(tk, &max))
        return default_value;

    if (!ExpectDelimiter(tk, ']'))
        return default_value;

    return { Min(min,max), Max(min,max) };
}

static bool ParseColor(Tokenizer& tk, VfxColor* value)
{
    if (!ExpectDelimiter(tk, '['))
    {
        Color cvalue = {1.0f, 1.0f, 1.0f, 1.0f};
        if (!ExpectColor(tk, &cvalue))
            return false;

        *value = {cvalue, cvalue};
        return true;
    }

    // Range
    Color min = {0,0,0,0};
    if (!ExpectColor(tk, &min))
        return false;

    if (!ExpectDelimiter(tk, ','))
        return false;

    Color max = {0,0,0,0};
    if (!ExpectColor(tk, &max))
        return false;

    if (!ExpectDelimiter(tk, ']'))
        return false;

    *value = { min, max };
    return true;
}

static VfxColorCurve ParseColorCurve(const std::string& str, const VfxColorCurve& default_value)
{
    Tokenizer tk;
    Init(tk, str.c_str());

    VfxColorCurve value = { VFX_CURVE_TYPE_LINEAR };
    if (!ParseColor(tk, &value.start))
        return default_value;

    if (!ExpectDelimiter(tk, '='))
    {
        value.end = value.start;
        return value;
    }

    if (!ExpectDelimiter(tk, '>'))
        return default_value;

    if (!ParseColor(tk, &value.end))
        return default_value;

    if (!ExpectDelimiter(tk, ':'))
        return value;

    VfxCurveType curve_type = VFX_CURVE_TYPE_UNKNOWN;
    if (!ParseCurveType(tk, &curve_type))
        return default_value;

    value.type = curve_type;
    return value;
}

static Bounds2 CalculateBounds(const EditorVfx& evfx)
{
    Bounds2 bounds;
    for (int i=0, c=evfx.emitter_count; i<c; i++)
    {
        const VfxEmitterDef& e = evfx.emitters[i].def;
        const VfxParticleDef& p = e.particle_def;
        Bounds2 eb = { e.spawn.min, e.spawn.max };
        float ssmax = Max(p.size.start.min,p.size.start.max);
        float semax = Max(p.size.end.min,p.size.end.max);
        float smax = Max(ssmax, semax);
        eb = Expand(eb, smax);

        float speed_max = Max(p.speed.start.max, p.speed.end.max);
        float duration_max = p.duration.max;
        eb = Expand(eb, speed_max * duration_max);

        if (i == 0)
            bounds = eb;
        else
            bounds = Union(bounds, eb);
    }

    return bounds;
}

void Serialize(const EditorVfx& evfx, Stream* stream)
{
    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE_VFX;
    header.version = 1;
    header.flags = 0;
    WriteAssetHeader(stream, &header);

    WriteStruct<Bounds2>(stream, CalculateBounds(evfx));

    WriteStruct(stream, evfx.duration);
    WriteBool(stream, evfx.loop);

    WriteU32(stream, evfx.emitter_count);
    for (int i=0, c=evfx.emitter_count; i<c; i++)
    {
        const EditorVfxEmitter& emitter = evfx.emitters[i];
        WriteStruct(stream, emitter.def.rate);
        WriteStruct(stream, emitter.def.burst);
        WriteStruct(stream, emitter.def.duration);
        WriteStruct(stream, emitter.def.angle);
        //WriteStruct(stream, emitter.def.radius);
        WriteStruct(stream, emitter.def.spawn);

        const VfxParticleDef& particle = emitter.def.particle_def;
        WriteStruct(stream, particle.duration);
        WriteStruct(stream, particle.size);
        WriteStruct(stream, particle.speed);
        WriteStruct(stream, particle.color);
        WriteStruct(stream, particle.opacity);
        WriteStruct(stream, particle.gravity);
        WriteStruct(stream, particle.drag);
        WriteStruct(stream, particle.rotation);
    }
}

Vfx* ToVfx(Allocator* allocator, const EditorVfx& evfx, const Name* name)
{
    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 8192);
    if (!stream)
        return nullptr;
    Serialize(evfx, stream);
    SeekBegin(stream, 0);

    Vfx* vfx = (Vfx*)LoadAssetInternal(allocator, name, ASSET_SIGNATURE_VFX, LoadVfx, stream);
    Free(stream);

    return vfx;
}

EditorVfx* LoadEditorVfx(Allocator* allocator, const std::filesystem::path& source_path)
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

    EditorVfx* ex = (EditorVfx*)Alloc(allocator, sizeof(EditorVfx));
    ex->duration = ParseFloat(source->GetString("VFX", "duration", "5.0"), {5,5});
    ex->loop = source->GetBool("vfx", "loop", false);

    auto emitter_names = source->GetKeys("emitters");
    for (const auto& emitter_name : emitter_names)
    {
        if (!source->HasGroup(emitter_name.c_str()))
            throw std::exception((std::string("missing emitter ") + emitter_name).c_str());

        std::string particle_section = emitter_name + ".particle";
        if (!source->HasGroup(particle_section.c_str()))
            throw std::exception((std::string("missing particle ") + particle_section).c_str());

        // Emitter
        EditorVfxEmitter& emitter = ex->emitters[ex->emitter_count++];;
        emitter.name = GetName(emitter_name.c_str());
        emitter.def.rate = ParseInt(source->GetString(emitter_name.c_str(), "rate", "0"), VFX_INT_ZERO);
        emitter.def.burst = ParseInt(source->GetString(emitter_name.c_str(), "burst", "0"), VFX_INT_ZERO);
        emitter.def.duration = ParseFloat(source->GetString(emitter_name.c_str(), "duration", "1.0"), VFX_FLOAT_ONE);
        emitter.def.angle = ParseFloat(source->GetString(emitter_name.c_str(), "angle", "0..360"), {0.0f, 360.0f});
        //emitter.def.radius = ParseFloat(source->GetString(emitter_name.c_str(), "radius", "0"), VFX_FLOAT_ZERO);
        emitter.def.spawn = ParseVec2(source->GetString(emitter_name.c_str(), "spawn", "(0, 0, 0)"), VFX_VEC2_ZERO);

        // Particle
        emitter.def.particle_def.duration = ParseFloat(source->GetString(particle_section.c_str(), "duration", "1.0"), VFX_FLOAT_ONE);
        emitter.def.particle_def.size = ParseFloatCurve(source->GetString(particle_section.c_str(), "size", "1.0"), VFX_FLOAT_CURVE_ONE);
        emitter.def.particle_def.speed = ParseFloatCurve(source->GetString(particle_section.c_str(), "speed", "0"), VFX_FLOAT_CURVE_ZERO);
        emitter.def.particle_def.color = ParseColorCurve(source->GetString(particle_section.c_str(), "color", "white"), VFX_COLOR_CURVE_WHITE);
        emitter.def.particle_def.opacity = ParseFloatCurve(source->GetString(particle_section.c_str(), "opacity", "1.0"), VFX_FLOAT_CURVE_ONE);
        emitter.def.particle_def.gravity = ParseVec2(source->GetString(particle_section.c_str(), "gravity", "(0, 0, 0)"), VFX_VEC2_ZERO);
        emitter.def.particle_def.drag = ParseFloat(source->GetString(particle_section.c_str(), "drag", "0"), VFX_FLOAT_ZERO);
        emitter.def.particle_def.rotation = ParseFloatCurve(source->GetString(particle_section.c_str(), "rotation", "0.0"), VFX_FLOAT_CURVE_ZERO);
    }

    return ex;
}

EditorAsset* LoadEditorVfxAsset(const std::filesystem::path& path)
{
    EditorVfx* evfx = LoadEditorVfx(ALLOCATOR_DEFAULT, path);
    if (!evfx)
        return nullptr;

    EditorAsset* ea = CreateEditableAsset(path, EDITOR_ASSET_TYPE_VFX);
    ea->vfx = evfx;
    ea->vfx->vfx = ToVfx(ALLOCATOR_DEFAULT, *evfx, ea->name);
    ea->vfx_handle = INVALID_VFX_HANDLE;
    return ea;
}

EditorVfx* Clone(Allocator* allocator, const EditorVfx& ev)
{
    EditorVfx* clone = (EditorVfx*)Alloc(allocator, sizeof(EditorVfx));
    *clone = ev;
    clone->vfx = nullptr;
    return clone;
}
