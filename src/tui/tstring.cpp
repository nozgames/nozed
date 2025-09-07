//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "tstring.h"
#include "screen.h"
#include "tokenizer.h"

struct TStringImpl : TString
{
    size_t length;
    size_t capacity;
};

struct TStringBuilderImpl : TStringBuilder
{
    size_t length;
    size_t capacity;
    TChar* buffer;
};

static TStringImpl* Impl(TString* s) { return (TStringImpl*)s; }
static TStringBuilderImpl* Impl(TStringBuilder* s) { return (TStringBuilderImpl*)s; }

#if 0
TStringBuilder& TStringBuilder::Add(const std::string& text)
{
    if (!_color_stack.empty())
    {
        // Use current color from stack
        const Color24& color = _color_stack.back();
        return Add(text, color.r, color.g, color.b);
    }

    _raw += text;
    _formatted += text;
    return *this;
}

TStringBuilder& TStringBuilder::Add(const char* text)
{
    Add(std::string(text));
    return *this;
}

TStringBuilder& TStringBuilder::Add(const std::string& text, const Color24& color)
{
    return Add(text, color.r, color.g, color.b);
}

TStringBuilder& TStringBuilder::Add(const std::string& text, int r, int g, int b)
{
    // Clamp RGB values
    r = std::max(0, std::min(255, r));
    g = std::max(0, std::min(255, g));
    b = std::max(0, std::min(255, b));
    
    _formatted += "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
    _formatted += text;
    _formatted += "\033[0m";
    _raw += text;
    return *this;
}

TStringBuilder& TStringBuilder::Add(const std::string& text, int tcolor)
{
    if (tcolor <= 0)
        return Add(text);

    _formatted += "\033[" + std::to_string(tcolor) + "m";
    _formatted += text;
    _formatted += "\033[0m";
    _raw += text;
    return *this;
}

TStringBuilder& TStringBuilder::PushColor(const Color24& color)
{
    _color_stack.push_back(color);
    return *this;
}

TStringBuilder& TStringBuilder::PushColor(int r, int g, int b)
{
    _color_stack.push_back(Color24{static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b)});
    return *this;
}

TStringBuilder& TStringBuilder::PopColor()
{
    if (!_color_stack.empty())
    {
        _color_stack.pop_back();
    }
    return *this;
}

TStringBuilder& TStringBuilder::Clear()
{
    _raw.clear();
    _formatted.clear();
    _color_stack.clear();
    return *this;
}

#endif

#if 0
TStringBuilder& TStringBuilder::TruncateToWidth(size_t max_width)
{
    if (_raw.size() <= max_width)
        return *this;
        
    // Need to truncate while preserving ANSI sequences
    size_t visual_len = 0;
    size_t truncate_pos = 0;
    
    for (size_t i = 0; i < _formatted.length(); i++)
    {
        if (_formatted[i] == '\033' && i + 1 < _formatted.length() && _formatted[i + 1] == '[')
        {
            // Skip ANSI escape sequence
            i++;
            while (i < _formatted.length() && _formatted[i] != 'm')
                i++;
            // Don't increment visual_len for ANSI codes
        }
        else
        {
            visual_len++;
            if (visual_len >= max_width)
            {
                truncate_pos = i + 1;
                break;
            }
        }
        truncate_pos = i + 1;
    }
    
    _formatted = _formatted.substr(0, truncate_pos);
    _formatted += "\033[0m"; // Ensure we end with a reset
    return *this;
}
#endif

#if 0
// Type-specific Add overloads
TStringBuilder& TStringBuilder::Add(const TString& tstr)
{
    _raw += tstr.raw;
    _formatted += tstr.formatted;
    return *this;
}

TStringBuilder& TStringBuilder::Add(const vec2& v)
{
    Add("(", TCOLOR_GREY).Add(v.x)
        .Add(", ", TCOLOR_GREY).Add(v.y)
        .Add(")", TCOLOR_GREY);
    return *this;
}

TStringBuilder& TStringBuilder::Add(const vec3& v)
{
    Add("(", TCOLOR_GREY).Add(v.x)
        .Add(", ", TCOLOR_GREY).Add(v.y)
        .Add(", ", TCOLOR_GREY).Add(v.z)
        .Add(")", TCOLOR_GREY);
    return *this;
}

TStringBuilder& TStringBuilder::Add(const vec4& v)
{
    Add("(", TCOLOR_GREY).Add(v.x)
        .Add(", ", TCOLOR_GREY).Add(v.y)
        .Add(", ", TCOLOR_GREY).Add(v.z)
        .Add(", ", TCOLOR_GREY).Add(v.w)
        .Add(")", TCOLOR_GREY);
    return *this;
}

TStringBuilder& TStringBuilder::Add(const Color24& color)
{
    char hex[8];
    snprintf(hex, sizeof(hex), "#%02X%02X%02X", color.r, color.g, color.b);

    auto hex_str = std::string(hex);
    
    // Add color block with background color followed by hex text
    _formatted += "\033[48;2;" + std::to_string(color.r) + ";" + std::to_string(color.g) + ";" + std::to_string(color.b) + "m \033[0m";
    _formatted += " " + hex_str;
    _raw += hex_str;
    return *this;
}

TStringBuilder& TStringBuilder::Add(const color_t& color)
{
    // Convert color_t to color24_t
    Color24 color24;
    color24.r = static_cast<uint8_t>(color.r * 255);
    color24.g = static_cast<uint8_t>(color.g * 255);
    color24.b = static_cast<uint8_t>(color.b * 255);
    
    return Add(color24);
}

TStringBuilder& TStringBuilder::Add(bool value)
{
    Add(value ? "true" : "false", TCOLOR_PURPLE);
    return *this;
}

TStringBuilder& TStringBuilder::Add(int value)
{
    Add(std::to_string(value), TCOLOR_ORANGE);
    return *this;
}

TStringBuilder& TStringBuilder::Add(float value)
{
    char buffer[32];
    snprintf(buffer, sizeof(buffer), "%g", value);
    Add(std::string(buffer), TCOLOR_ORANGE);
    return *this;
}

#endif

static char* RenderEscape(char* o)
{
    *(o++) = '\033';
    *(o++) = '[';
    return o;
}

static char* RenderInt(char* o, int i)
{
    //auto len = snprintf(o, OUTPUT_BUFFER_SIZE - (o - g_output_buffer)-1, "%d", i);
    auto len = snprintf(o, 1024, "%d", i);
    o += len;;
    return o;
}

static char* RenderColor(char* o, TColor color)
{
#if 0
    if (color.code == 0)
    {
        o = RenderEscape(o);
        *o++ = '3';
        *o++ = '9';
        *o++ = 'm';
        return o;
    }

    auto color24 = GetTColor(color);

    o = RenderEscape(o);
    *o++ = '3';
    *o++ = '8';
    *o++ = ';';
    *o++ = '2';
    *o++ = ';';
    o = RenderInt(o, color24.r);
    *o++ = ';';
    o = RenderInt(o, color24.g);
    *o++ = ';';
    o = RenderInt(o, color24.b);
    *o++ = 'm';
#endif
    return o;
}

static char* RenderBackgroundColor(char* o, TColor color)
{
#if 0
    if (color.code == 0)
    {
        o = RenderEscape(o);
        *o++ = '4';
        *o++ = '9';
        *o++ = 'm';
        return o;
    }

    auto color24 = GetTColor(color);

    o = RenderEscape(o);
    *o++ = '4';
    *o++ = '8';
    *o++ = ';';
    *o++ = '2';
    *o++ = ';';
    o = RenderInt(o, color24.r);
    *o++ = ';';
    o = RenderInt(o, color24.g);
    *o++ = ';';
    o = RenderInt(o, color24.b);
    *o++ = 'm';
#endif
    return o;
}

TStringBuilder* Append(TStringBuilder* builder, const char* text, TColor fg_color, TColor bg_color)
{
    return builder;
}

TStringBuilder* CreateTStringBuilder(Allocator* allocator, size_t capacity)
{
    auto builder = (TStringBuilder*)Alloc(allocator, sizeof(TStringBuilderImpl) + sizeof(TChar) * capacity);
    auto impl = Impl(builder);
    impl->length = 0;
    impl->capacity = capacity;
    impl->buffer = (TChar*)(impl + 1);
    return builder;
}

TString* CreateTString(Allocator* allocator, const TChar* data, size_t data_len)
{
    auto tstr = (TString*)Alloc(allocator, sizeof(TStringImpl) + sizeof(TChar) * data_len);
    auto impl = Impl(tstr);
    impl->length = data_len;
    impl->capacity = data_len;
    memcpy(impl + 1, data, data_len * sizeof(TChar));
    return tstr;
}

TString* CreateTString(Allocator* allocator, size_t capacity)
{
    auto tstr = (TString*)Alloc(allocator, sizeof(TStringImpl) + sizeof(TChar) * capacity);
    auto impl = Impl(tstr);
    impl->length = 0;
    impl->capacity = capacity;
    return tstr;
}


u32 CStringToTChar(const char* src, TChar* dst, u32 dst_size, TColor fg, TColor bg)
{
    TColor fg_color = fg;
    TColor bg_color = bg;
    u32 length = 0;

    Token token;
    Tokenizer tk;
    Init(tk, src);

    while (HasTokens(tk))
    {
        char c = PeekChar(tk);
        if (c != '\033')
        {
            *dst = { c, fg_color, bg_color };
            dst++;
            dst_size--;
            length++;
            NextChar(tk);
            continue;
        }

        NextChar(tk);

        if (!ExpectChar(tk, '['))
            continue;

        if (ExpectChar(tk, 'm'))
        {
            fg_color = fg;
            bg_color = bg;
            continue;
        }

        bool expect_semi = false;

        while (HasTokens(tk) && !ExpectChar(tk, 'm'))
        {
            if (expect_semi && !ExpectChar(tk, ';'))
                break;

            expect_semi = true;

            int value = 0;
            if (!ExpectInt(tk, &token, &value))
                continue;

            switch (value)
            {
            case 0:
                fg_color = fg;
                bg_color = bg;
                break;

            case 30:
            case 31:
            case 32:
            case 33:
            case 34:
            case 35:
            case 36:
            case 37:
                fg_color = { (u8)value, 0, 0, 0 };
                break;

            case 38:
                break;

            case 39:
                fg_color = fg;
                break;

            case 40:
            case 41:
            case 42:
            case 43:
            case 44:
            case 45:
            case 46:
            case 47:
                bg_color = { (u8)value, 0, 0, 0 };
                break;

            case 48:
                break;

            case 49:
                fg_color = bg;
                break;
            }

            if (value == 38 || value == 48)
            {
                i32 r;
                i32 g;
                i32 b;
                bool success = ExpectChar(tk, ';');
                success = success && ExpectChar(tk, '2');
                success = success && ExpectChar(tk, ';');
                success = success && ExpectInt(tk, &token, &r);
                success = success && ExpectChar(tk, ';');
                success = success && ExpectInt(tk, &token, &g);
                success = success && ExpectChar(tk, ';');
                success = success && ExpectInt(tk, &token, &b);
                if (success)
                {
                    if (value == 38)
                        fg_color = { (u8)value, (u8)r, (u8)g, (u8)b};
                    else
                        bg_color = { (u8)value, (u8)r, (u8)g, (u8)b};
                }
            }
        }
    }

    return length;
}