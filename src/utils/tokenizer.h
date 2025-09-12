//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

#include <noz/color.h>

// @TokenType
enum TokenType
{
    TOKEN_TYPE_NONE,
    TOKEN_TYPE_INT,
    TOKEN_TYPE_FLOAT,
    TOKEN_TYPE_STRING,
    TOKEN_TYPE_IDENTIFIER,
    TOKEN_TYPE_VEC2,
    TOKEN_TYPE_VEC3,
    TOKEN_TYPE_VEC4,
    TOKEN_TYPE_DELIMITER,
    TOKEN_TYPE_COLOR,
    TOKEN_TYPE_BOOL,
    TOKEN_TYPE_EOF
};

struct Token
{
    const char* raw;
    size_t length;
    size_t line;
    size_t column;
    TokenType type;
    union
    {
        float f;
        int i;
        bool b;
        Color c;
        Vec2 v2;
        Vec3 v3;
        Vec4 v4;
    } value;
};

struct Tokenizer
{
    const char* input;
    size_t position;
    size_t length;
    size_t line;
    size_t column;
    Token next_token;
    Token current_token;
};

// @tokenizer
extern void Init(Tokenizer& tk, const char* input);
extern const Name* GetName(const Tokenizer& tk);
extern char* GetString(const Tokenizer& tk, char* dst, u32 dst_size);
extern char* GetString(const Tokenizer& tk);
extern bool Equals(Tokenizer& tk, const char* value, bool ignore_case=false);
extern bool Equals(Tokenizer& tk, TokenType type);
extern bool ExpectLine(Tokenizer& tk);
extern bool IsEOF(Tokenizer& tk);

extern bool ExpectQuotedString(Tokenizer &tk);
extern bool ExpectInt(Tokenizer& tk, int* out_value);
extern bool ExpectFloat(Tokenizer& tk, float* out_value);
extern bool ExpectIdentifier(Tokenizer& tk, const char* value = nullptr);
extern bool ExpectVec2(Tokenizer& tk, Vec2* out_value=nullptr);
extern bool ExpectVec3(Tokenizer& tk, Vec3* out_value=nullptr);
extern bool ExpectVec4(Tokenizer& tk, Vec4* out_value=nullptr);
extern bool ExpectColor(Tokenizer& tk, Color* out_value=nullptr);
extern bool ExpectDelimiter(Tokenizer& tk, char c);

#if defined(_STRING_)
std::string ToString(const Token& token);
#endif
