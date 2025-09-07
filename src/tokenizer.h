//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

#include <noz/color.h>
#include <noz/text.h>

// @TokenType
enum TokenType
{
    TOKEN_TYPE_NONE,
    TOKEN_TYPE_NUMBER,
    TOKEN_TYPE_STRING,
    TOKEN_TYPE_VEC2,
    TOKEN_TYPE_VEC3,
    TOKEN_TYPE_VEC4,
    TOKEN_TYPE_OPERATOR,       // Operators (+, -, *, /, =, etc.)
    TOKEN_TYPE_DELIMITER,      // Delimiters (, ), {, }, [, ], ;, :, etc.
    TOKEN_TYPE_COLOR,
    TOKEN_TYPE_EOF
};

struct Token
{
    const char* value;
    size_t length;
    size_t line;
    size_t column;
    TokenType type;
};

struct Tokenizer
{
    const char* input;
    size_t position;
    size_t length;
    size_t line;
    size_t column;
};

// @tokenizer
void Init(Tokenizer& tok, const char* input);
bool HasTokens(Tokenizer& tok);
bool ExpectToken(Tokenizer& tok, TokenType type, Token* token);
bool ExpectQuotedString(Tokenizer& tok, Token* token);
bool ExpectIdentifier(Tokenizer& tok, Token* result);
bool ExpectNumber(Tokenizer& tok, Token* result);
bool ExpectFloat(Tokenizer& tok, Token* token, float* result);
bool ExpectInt(Tokenizer& tok, Token* token, int* result);
bool ExpectVec2(Tokenizer& tok, Token* token, Vec2* result);
bool ExpectVec3(Tokenizer& tok, Token* token, Vec3* result);
bool ExpectVec4(Tokenizer& tok, Token* token, Vec4* result);
bool ExpectColor(Tokenizer& tok, Token* token, Color* result);
bool ReadUntil(Tokenizer& tok, Token* token, char c, bool multiline);
bool ReadLine(Tokenizer& tok, Token* token);
bool NextToken(Tokenizer& tok, Token* token);
void SkipWhitespace(Tokenizer& tok);
void SkipLine(Tokenizer* tok);
char PeekChar(Tokenizer& tok);
char NextChar(Tokenizer& tok);
bool ExpectChar(Tokenizer& tok, char expected);

// @token
void InitToken(Token* token);
void ClearToken(Token* token);
bool IsTokenType(Token* token, TokenType type);
bool IsValue(const Token& token, const char* value, bool ignore_case=false);


#if defined(_STRING_)
std::string ToString(const Token& token);
#endif
