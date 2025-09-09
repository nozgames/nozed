//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "tokenizer.h"
#include <string>

static void ClearToken(Token* token);
static bool IsOperator(char c);
static bool IsDelimiter(char c);
static bool IsIdentifier(char c, bool first_char);
static bool IsNumber(char c);
static bool IsWhitespace(char c);

void Init(Tokenizer& tok, const char* input)
{
    tok.input = input;
    tok.position = 0;
    tok.length = input ? strlen(input) : 0;
    tok.line = 1;
    tok.column = 1;
}

bool HasTokens(Tokenizer& tok)
{
    return tok.position < tok.length;
}

char PeekChar(Tokenizer& tok)
{
    if (!HasTokens(tok))
        return '\0';

    return tok.input[tok.position];
}

char NextChar(Tokenizer& tok)
{
    if (!HasTokens(tok))
        return '\0';

    char c = tok.input[tok.position++];
    if (c == '\n')
    {
        tok.line++;
        tok.column = 1;
    }
    else
    {
        tok.column++;
    }

    return c;
}

bool ExpectChar(Tokenizer& tok, char expected)
{
    SkipWhitespace(tok);
    if (PeekChar(tok) != expected)
        return false;
    NextChar(tok);
    return true;
}

void SkipWhitespace(Tokenizer& tok)
{
    while (HasTokens(tok) && isspace(PeekChar(tok)) && PeekChar(tok) != '\n')
        NextChar(tok);
}

void SkipLine(Tokenizer& tok)
{
    while (HasTokens(tok) && PeekChar(tok) != '\n')
        NextChar(tok);

    if (PeekChar(tok) == '\n')
        NextChar(tok);
}

void BeginToken(Tokenizer& tok, Token* result)
{
    result->line = tok.line;
    result->column = tok.column;
    result->value = tok.input + tok.position;
    result->length = 0;
}

void EndToken(Tokenizer& tok, Token* result, TokenType type)
{
    result->length = tok.position - (result->value - tok.input);
    result->type = type;
}

bool ExpectQuotedString(Tokenizer& tok, Token* result)
{
    ClearToken(result);

    char quote_char = PeekChar(tok);
    if (quote_char != '"' && quote_char != '\'')
        return false;

    // Skip opening quote
    NextChar(tok);

    BeginToken(tok, result);

    while (HasTokens(tok))
    {
        char c = NextChar(tok);

        if (c == quote_char)
        {
            EndToken(tok, result, TOKEN_TYPE_STRING);
            result->length--;
            return true;
        }

        if (c == '\\' && HasTokens(tok))
        {
            // Escape sequence
            char escaped = NextChar(tok);
            switch (escaped)
            {
                case 'n':  c = '\n'; break;
                case 't':  c = '\t'; break;
                case 'r':  c = '\r'; break;
                case '\\': c = '\\'; break;
                case '"':  c = '"';  break;
                case '\'': c = '\''; break;
                default:   c = escaped; break;
            }
        }
    }

    return false;
}

bool ExpectIdentifier(Tokenizer& tok, Token* result)
{
    ClearToken(result);

    SkipWhitespace(tok);

    if (!IsIdentifier(PeekChar(tok), true))
        return false;

    BeginToken(tok, result);

    while (IsIdentifier(PeekChar(tok), false))
        NextChar(tok);

    EndToken(tok, result, TOKEN_TYPE_STRING);

    return result->length > 0;
}

bool ExpectNumber(Tokenizer& tok, Token* result)
{
    ClearToken(result);
    SkipWhitespace(tok);

    BeginToken(tok, result);

    bool has_digits = false;
    bool has_decimal = false;

    while (IsNumber(PeekChar(tok)))
    {
        char c = NextChar(tok);

        auto is_digit = (isdigit(c) != 0);
        has_digits |= is_digit;

        if (has_decimal && c == '.')
            return false;

        has_decimal |= c == '.';
    }

    EndToken(tok, result, TOKEN_TYPE_NUMBER);

    if (!has_digits || result->length < 1)
    {
        ClearToken(result);
        return false;
    }

    return true;
}

bool ExpectFloat(Tokenizer& tok, Token* token, float* result)
{
    assert(token);
    assert(result);

    if (!ExpectNumber(tok, token))
        return false;

    try
    {
        if (token->length > 64)
            return false;

        char number_str[65];
        strncpy(number_str, token->value, token->length);
        number_str[token->length] = '\0';

        *result = (float)atof(number_str);
        return true;
    }
    catch (...)
    {
    }

    return false;
}

bool ExpectInt(Tokenizer& tok, Token* token, int* result)
{
    assert(token);
    assert(result);

    if (!ExpectNumber(tok, token))
        return false;

    try
    {
        if (token->length > 64)
            return false;

        char number_str[65];
        strncpy(number_str, token->value, token->length);
        number_str[token->length] = '\0';

        *result = (float)atoi(number_str);
        return true;
    }
    catch (...)
    {
    }

    return false;
}

bool ExpectVec2(Tokenizer& tok, Token* token, Vec2* result)
{
    assert(token);
    assert(result);

    SkipWhitespace(tok);

    BeginToken(tok, token);

    // Single value (no parens)
    if (ExpectFloat(tok, token, &result->x))
    {
        EndToken(tok, token, TOKEN_TYPE_VEC2);
        result->y = result->x;
        return true;
    }

    if (!ExpectChar(tok, '('))
        return false;

    Token temp = {};
    if (!ExpectFloat(tok, &temp, &result->x))
        return false;

    if (!ExpectChar(tok, ','))
        return false;

    if (!ExpectFloat(tok, &temp, &result->y))
        return false;

    if (!ExpectChar(tok, ')'))
        return false;

    EndToken(tok, token, TOKEN_TYPE_VEC2);

    return true;
}


bool ExpectVec3(Tokenizer& tok, Token* token, Vec3* result)
{
    assert(token);
    assert(result);

    SkipWhitespace(tok);

    BeginToken(tok, token);

    if (!ExpectChar(tok, '('))
        return false;

    Token temp = {};
    if (!ExpectFloat(tok, &temp, &result->x))
        return false;

    if (!ExpectChar(tok, ','))
        return false;

    if (!ExpectFloat(tok, &temp, &result->y))
        return false;

    if (!ExpectChar(tok, ','))
        return false;

    if (!ExpectFloat(tok, &temp, &result->z))
        return false;

    if (!ExpectChar(tok, ')'))
        return false;

    EndToken(tok, token, TOKEN_TYPE_VEC3);

    return true;
}

bool ExpectVec4(Tokenizer& tok, Token* token, Vec4* result)
{
    assert(token);
    assert(result);

    SkipWhitespace(tok);

    BeginToken(tok, token);

    if (!ExpectChar(tok, '('))
        return false;

    Token temp = {};
    if (!ExpectFloat(tok, &temp, &result->x))
        return false;

    if (!ExpectChar(tok, ','))
        return false;

    if (!ExpectFloat(tok, &temp, &result->y))
        return false;

    if (!ExpectChar(tok, ','))
        return false;

    if (!ExpectFloat(tok, &temp, &result->z))
        return false;

    if (!ExpectChar(tok, ','))
        return false;

    if (!ExpectFloat(tok, &temp, &result->w))
        return false;

    if (!ExpectChar(tok, ')'))
        return false;

    EndToken(tok, token, TOKEN_TYPE_VEC4);

    return true;
}

static void ClearToken(Token* token)
{
    assert(token);

    token->type = TOKEN_TYPE_NONE;
    token->value = "";
    token->line = 0;
    token->column = 0;
}

static bool IsOperator(char c)
{
    return c == '+' || c == '-' || c == '*' || c == '/' || c == '=' ||
           c == '<' || c == '>' || c == '!' || c == '&' || c == '|' ||
           c == '^' || c == '%' || c == '~';
}

static bool IsDelimiter(char c)
{
    return c == '(' || c == ')' || c == '{' || c == '}' ||
           c == '[' || c == ']' || c == ';' || c == ':' ||
           c == ',' || c == '.' || c == '#';
}

static bool IsIdentifier(char c, bool first_char)
{
    if (first_char)
        return isalpha(c) || c == '_';
    return isalnum(c) || c == '_' || c == ':';
}

static bool IsNumber(char c)
{
    return isdigit(c) || c == '-' || c == '+' || c == '.';
}

static bool IsWhitespace(char c)
{
    return isspace(c) || c == '\n' || c == '\r';
}

bool ReadLine(Tokenizer& tok, Token* token)
{
    while (HasTokens(tok))
    {
        SkipWhitespace(tok);
        BeginToken(tok, token);
        while (HasTokens(tok) && PeekChar(tok) != '\n')
            NextChar(tok);

        // Skip EOL
        EndToken(tok, token, TOKEN_TYPE_NONE);
        NextChar(tok);

        // Trim the end
        while (token->length > 0 && IsWhitespace(token->value[token->length - 1]))
            token->length--;

        if (token->length > 0)
            return true;
    }

    return false;
}

bool ReadUntil(Tokenizer& tok, Token* token, char c, bool multiline)
{
    SkipWhitespace(tok);
    BeginToken(tok, token);
    while (HasTokens(tok))
    {
        auto peek =  PeekChar(tok);
        if (peek == c)
        {
            EndToken(tok, token, TOKEN_TYPE_NONE);
            return true;
        }

        if (!multiline && peek == '\n')
        {
            EndToken(tok, token, TOKEN_TYPE_NONE);
            return false;
        }

        NextChar(tok);
    }

    EndToken(tok, token, TOKEN_TYPE_NONE);

    return false;
}

bool NextToken(Tokenizer& tok, Token* token)
{
    assert(token);

    ClearToken(token);
    SkipWhitespace(tok);

    if (!HasTokens(tok))
    {
        BeginToken(tok, token);
        EndToken(tok, token, TOKEN_TYPE_EOF);
        return false;
    }

    BeginToken(tok, token);

    char c = PeekChar(tok);

    if (c == '"' || c == '\'')
        return ExpectQuotedString(tok, token);

    if (IsNumber(c))
        return ExpectNumber(tok, token);

    if (IsIdentifier(c, true))
        return ExpectIdentifier(tok, token);

    // Operators
    if (IsOperator(c))
    {
        BeginToken(tok, token);
        NextChar(tok);
        EndToken(tok, token, TOKEN_TYPE_OPERATOR);
        return true;
    }

    // Delimiters
    if (IsDelimiter(c))
    {
        BeginToken(tok, token);
        NextChar(tok);
        EndToken(tok, token, TOKEN_TYPE_OPERATOR);
        return true;
    }

    // Color?
    if (c == '#')
    {
        Color col = {};
        return ExpectColor(tok, token, &col);
    }

    BeginToken(tok, token);
    NextChar(tok);
    EndToken(tok, token, TOKEN_TYPE_NONE);
    return true;
}

bool IsTokenType(Token* token, TokenType type)
{
    return token && token->type == type;
}

bool IsTokenValue(const Token& token, const char* value)
{
    return value && strcmp(token.value, value) == 0;
}

bool ExpectColor(Tokenizer& tok, Token* token, Color* result)
{
    assert(result);

    SkipWhitespace(tok);

    BeginToken(tok, token);

    char c = PeekChar(tok);

    // Handle hex colors: #RRGGBB or #RRGGBBAA
    if (PeekChar(tok) == '#')
    {
        NextChar(tok);
        while (isxdigit(PeekChar(tok)))
            NextChar(tok); // Skip #

        EndToken(tok, token, TOKEN_TYPE_COLOR);

        if (token->length > 9)
            return false;

        // Extract hex to cstr
        char hex_str[16];
        strncpy(hex_str, token->value + 1, token->length - 1);
        hex_str[token->length - 1] = '\0';

        if (token->length == 7) // #RRGGBB
        {
            auto hex = (unsigned int)strtoul(hex_str, nullptr, 16);
            result->r = ((hex >> 16) & 0xFF) / 255.0f;
            result->g = ((hex >> 8) & 0xFF) / 255.0f;
            result->b = (hex & 0xFF) / 255.0f;
            result->a = 1.0f;
            return true;
        }

        if (token->length == 9) // #RRGGBBAA
        {
            unsigned int hex = (unsigned int)strtoul(hex_str, nullptr, 16);
            result->r = ((hex >> 24) & 0xFF) / 255.0f;
            result->g = ((hex >> 16) & 0xFF) / 255.0f;
            result->b = ((hex >> 8) & 0xFF) / 255.0f;
            result->a = (hex & 0xFF) / 255.0f;
            return true;
        }

        if (token->length == 4) // #RGB shorthand
        {
            unsigned int hex = (unsigned int)strtoul(hex_str, nullptr, 16);
            result->r = ((hex >> 8) & 0xF) / 15.0f;
            result->g = ((hex >> 4) & 0xF) / 15.0f;
            result->b = (hex & 0xF) / 15.0f;
            result->a = 1.0f;
            return true;
        }

        return false;
    }

    Token temp = {};
    if (!ExpectIdentifier(tok, &temp))
        return false;

    if (IsValue(temp, "rgba"))
    {
        Vec4 color = {};
        if (!ExpectVec4(tok, &temp, &color))
            return false;

        EndToken(tok, &temp, TOKEN_TYPE_COLOR);

        result->r = color.x / 255.0f;
        result->g = color.y / 255.0f;
        result->b = color.z / 255.0f;
        result->a = color.w;
        return true;
    }

    if (IsValue(temp, "rgb"))
    {
        Vec3 color = {};
        if (!ExpectVec3(tok, &temp, &color))
            return false;

        EndToken(tok, &temp, TOKEN_TYPE_COLOR);
        result->r = color.x / 255.0f;
        result->g = color.y / 255.0f;
        result->b = color.z / 255.0f;
        result->a = 1.0f;
        return true;
    }

    struct ColorName { const char* name; Color color; };
    static const ColorName predefined_colors[] = {
        {"black", {0.0f, 0.0f, 0.0f, 1.0f}},
        {"white", {1.0f, 1.0f, 1.0f, 1.0f}},
        {"red", {1.0f, 0.0f, 0.0f, 1.0f}},
        {"green", {0.0f, 0.5f, 0.0f, 1.0f}},
        {"blue", {0.0f, 0.0f, 1.0f, 1.0f}},
        {"yellow", {1.0f, 1.0f, 0.0f, 1.0f}},
        {"cyan", {0.0f, 1.0f, 1.0f, 1.0f}},
        {"magenta", {1.0f, 0.0f, 1.0f, 1.0f}},
        {"gray", {0.5f, 0.5f, 0.5f, 1.0f}},
        {"grey", {0.5f, 0.5f, 0.5f, 1.0f}},
        {"orange", {1.0f, 0.65f, 0.0f, 1.0f}},
        {"pink", {1.0f, 0.75f, 0.8f, 1.0f}},
        {"purple", {0.5f, 0.0f, 0.5f, 1.0f}},
        {"brown", {0.65f, 0.16f, 0.16f, 1.0f}},
        {"transparent", {0.0f, 0.0f, 0.0f, 0.0f}},
        {nullptr, {0.0f, 0.0f, 0.0f, 0.0f}}
    };

    for (const ColorName* color = predefined_colors; color->name != nullptr; color++)
        if (IsValue(temp, color->name))
        {
            *result = color->color;
            return true;
        }

    return false;
}

std::string ToString(const Token& token)
{
    if (token.length == 0)
        return "";

    auto start = token.value;
    auto end = token.value + token.length - 1;

    while (start < end && IsWhitespace(*start))
        start++;

    while (end > start && IsWhitespace(*end))
        end--;

    return std::string(start, end-start+1);
}

const Name* ToName(const Token& token)
{
    char name[MAX_NAME_LENGTH];
    ToString(token, name, MAX_NAME_LENGTH);
    return GetName(name);
}

char* ToString(const Token& token, char* dst, u32 dst_size)
{
    assert(dst);
    assert(dst_size > 0);

    if (token.length == 0)
    {
        *dst = '\0';
        return dst;
    }

    const char* start = token.value;
    const char* end = token.value + token.length - 1;

    while (start < end && IsWhitespace(*start))
        start++;

    while (end > start && IsWhitespace(*end))
        end--;

    Copy(dst, dst_size, start);
    return dst;
}

bool ExpectToken(Tokenizer& tok, TokenType type, Token* token)
{
    return NextToken(tok, token) && token->type == type;
}

bool IsValue(const Token& token, const char* value, bool ignore_case)
{
    if (ignore_case)
        return _strnicmp(token.value, value, token.length) == 0;

    return strncmp(token.value, value, token.length) == 0;
}
