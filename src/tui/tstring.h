//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

struct TColor
{
    uint8_t code;
    uint8_t r;
    uint8_t g;
    uint8_t b;

    bool operator== (const TColor& o) const { return code == o.code && r == o.r && g == o.g && b == o.b; }
    bool operator!= (const TColor& o) const { return !(*this == o); }
};

struct TChar
{
    char value;
    TColor fg_color;
    TColor bg_color;
};

struct TString {};
struct TStringBuilder {};

constexpr TColor TCOLOR_NONE = { 39, 0, 0, 0 };
constexpr TColor TCOLOR_RED = { 31, 0, 0, 0 };
constexpr TColor TCOLOR_BLACK = { 30, 0, 0, 0 };
constexpr TColor TCOLOR_WHITE = { 37, 0, 0, 0 };
constexpr TColor TCOLOR_STRING_VALUE = { 38, 0, 255, 0 };

constexpr TColor TCOLOR_BACKGROUND_NONE = { 49, 0, 0, 0 };
constexpr TColor TCOLOR_BACKGROUND_RED = { 41, 0, 0, 0 };
constexpr TColor TCOLOR_BACKGROUND_WHITE = { 47, 0, 0, 0 };
constexpr TChar TCHAR_NONE = { ' ', TCOLOR_NONE, TCOLOR_BACKGROUND_NONE };
constexpr TChar TCHAR_WHITE_BACKGROUND = { ' ', TCOLOR_NONE, TCOLOR_BACKGROUND_WHITE };

extern TString* CreateTString(Allocator* allocator);
extern TStringBuilder* CreateTStringBuilder(Allocator* allocator, size_t capacity = 8192);
extern TStringBuilder* Append(TStringBuilder* builder, const char* text, TColor fg_color = TCOLOR_NONE, TColor bg_color = TCOLOR_NONE);
extern u32 CStringToTChar(const char* src, TChar* dst, u32 dst_size, TColor fg = TCOLOR_NONE, TColor bg = TCOLOR_BACKGROUND_NONE);

#if 0

class TStringBuilder
{
    std::string _formatted;
    std::string _raw;
    std::vector<Color24> _color_stack;

public:

    // Builder pattern methods - all return *this for chaining
    TStringBuilder& Add(const char* text);
    TStringBuilder& Add(const std::string& text);     // Add text in current color from stack
    TStringBuilder& Add(const std::string& text, const Color24& color);  // Add text with specific color
    TStringBuilder& Add(const std::string& text, int r, int g, int b);  // Add text with RGB color
    TStringBuilder& Add(const std::string& text, int tcolor);  // Add text with RGB color

    // Type-specific overloads for common NoZ types
    TStringBuilder& Add(const TString& tstr);         // Add existing TString (text + visual length)
    TStringBuilder& Add(const vec2& v);               // Format as "(x, y)" 
    TStringBuilder& Add(const vec3& v);               // Format as "(x, y, z)"
    TStringBuilder& Add(const vec4& v);               // Format as "(x, y, z, w)"
    TStringBuilder& Add(const Color24& color);      // Format as hex color
    TStringBuilder& Add(const Color& color);        // Format as hex color (from float color)
    TStringBuilder& Add(bool value);                  // Format as "true"/"false"
    TStringBuilder& Add(int value);                   // Format as integer
    TStringBuilder& Add(float value);                 // Format as float
    
    // Color stack management
    TStringBuilder& PushColor(const Color24& color);
    TStringBuilder& PushColor(int r, int g, int b);
    TStringBuilder& PopColor();
    
    // Utility methods
    TStringBuilder& Clear();
    //TStringBuilder& TruncateToWidth(size_t max_width);
    
    // Query methods (const)
    size_t VisualLength() const { return _raw.length(); }
    bool Empty() const { return _formatted.empty(); }
    
    // Final build method
    TString ToTString() const;
    
    // Static factory method
    static TStringBuilder Build() { return TStringBuilder(); }
};

#endif
