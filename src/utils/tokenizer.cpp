//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "tokenizer.h"

static bool IsDelimiter(char c);
static bool IsIdentifier(char c, bool first_char);
static bool IsNumber(char c);
static bool IsWhitespace(char c);

static bool HasTokens(Tokenizer& tok)
{
    return tok.position < tok.length;
}

bool IsEOF(Tokenizer& tk)
{
    return tk.next_token.type == TOKEN_TYPE_EOF;
}

static void BeginToken(Tokenizer& tk)
{
    Token& token = tk.next_token;
    token.line = tk.line;
    token.column = tk.column;
    token.raw = tk.input + tk.position;
    token.length = 0;
}

static void EndToken(Tokenizer& tk, TokenType type)
{
    Token& token = tk.next_token;
    token.length = tk.position - (u32)(token.raw - tk.input);
    token.type = type;
}

static char PeekChar(Tokenizer& tk)
{
    if (!HasTokens(tk))
        return '\0';

    return tk.input[tk.position];
}

static char NextChar(Tokenizer& tok)
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

static void SkipWhitespace(Tokenizer& tk)
{
    while (HasTokens(tk))
    {
        char c = PeekChar(tk);
        if (!isspace(c) && c != '\n' && c != '\r')
            return;

        NextChar(tk);
    }
}

static bool IsDelimiter(char c)
{
    return c == '[' || c == ']' || c == '=' || c == ',' || c == '<' || c == '>' || c == ':';
}

static bool IsIdentifier(char c, bool first_char)
{
    if (first_char)
        return isalpha(c) || c == '_';
    return isalnum(c) || c == '_' || c == ':' || c == '/' || c == '-';
}

static bool IsNumber(char c)
{
    return isdigit(c) || c == '-' || c == '+' || c == '.';
}

static bool IsWhitespace(char c)
{
    return isspace(c) || c == '\n' || c == '\r';
}

const Name* GetName(const Tokenizer& tk)
{
    char name[MAX_NAME_LENGTH];
    GetString(tk, name, MAX_NAME_LENGTH);
    return GetName(name);
}

char* GetString(const Tokenizer& tk, char* dst, u32 dst_size)
{
    assert(dst);
    assert(dst_size > 0);

    Copy(dst, dst_size, tk.current_token.raw, tk.current_token.length);
    return dst;
}

char* GetString(const Token& token, char* dst, u32 dst_size)
{
    assert(dst);
    assert(dst_size > 0);

    Copy(dst, dst_size, token.raw, token.length);
    return dst;
}

static bool Equals(const Token& token, const char* value, bool ignore_case=false)
{
    return Equals(token.raw, value, token.length, ignore_case);
}

static bool Equals(const Token& token, TokenType type)
{
    return token.type == type;
}

bool Equals(Tokenizer& tk, const char* value, bool ignore_case)
{
    return Equals(tk.current_token, value, ignore_case);
}

bool Equals(Tokenizer& tk, TokenType type)
{
    return Equals(tk.current_token, type);
}

static bool ReadQuotedString(Tokenizer& tk)
{
    char quote_char = PeekChar(tk);
    if (quote_char != '"' && quote_char != '\'')
        return false;

    // Skip opening quote
    NextChar(tk);
    BeginToken(tk);

    while (HasTokens(tk))
    {
        char c = NextChar(tk);

        if (c == quote_char)
        {
            EndToken(tk, TOKEN_TYPE_STRING);
            tk.next_token.length--;
            return true;
        }

        // Escape sequence?
        if (c == '\\' && HasTokens(tk))
        {
            char escaped = NextChar(tk);
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

    EndToken(tk, TOKEN_TYPE_STRING);

    return true;
}

static bool ReadBool(Tokenizer& tk)
{
    if (Equals(tk.input + tk.position, "true", 4, true))
    {
        BeginToken(tk);
        for (int i = 0; i < 4; i++)
            NextChar(tk);
        EndToken(tk, TOKEN_TYPE_BOOL);
        tk.next_token.value.b = true;
        return true;
    }

    if (Equals(tk.input + tk.position, "false", 4, true))
    {
        BeginToken(tk);
        for (int i = 0; i < 4; i++)
            NextChar(tk);
        EndToken(tk, TOKEN_TYPE_BOOL);
        tk.next_token.value.b = true;
        return true;
    }

    return false;
}

bool ReadNumber(Tokenizer& tk)
{
    char c = PeekChar(tk);
    if (!IsNumber(c))
        return false;

    BeginToken(tk);

    bool has_digits = false;
    bool has_decimal = false;

    while (HasTokens(tk))
    {
        c = PeekChar(tk);
        if (!IsNumber(c))
            break;

        auto is_digit = isdigit(c) != 0;
        has_digits |= is_digit;

        if (has_decimal && c == '.')
            break;

        NextChar(tk);

        has_decimal |= c == '.';
    }

    EndToken(tk, has_decimal ? TOKEN_TYPE_FLOAT : TOKEN_TYPE_INT);

    char temp[128];
    Copy(temp, sizeof(temp), tk.next_token.raw, tk.next_token.length);
    if (has_decimal)
        tk.next_token.value.f = (f32)atof(temp);
    else
        tk.next_token.value.i = atoi(temp);

    return true;
}

bool ReadIdentifier(Tokenizer& tk)
{
    if (!IsIdentifier(PeekChar(tk), true))
        return false;

    BeginToken(tk);

    while (IsIdentifier(PeekChar(tk), false))
        NextChar(tk);

    EndToken(tk, TOKEN_TYPE_IDENTIFIER);

    return true;
}

static bool ReadVec(Tokenizer& tk, bool start_token=true)
{
    char c = PeekChar(tk);
    if (c != '(')
        return false;

    if (start_token)
        BeginToken(tk);

    Token saved_token = tk.next_token;

    SkipWhitespace(tk);
    NextChar(tk);

    int component_index = 0;
    Vec4 result = {};
    while (HasTokens(tk))
    {
        if (PeekChar(tk) == ')')
        {
            NextChar(tk);
            break;
        }

        ReadNumber(tk);

        if (tk.next_token.type == TOKEN_TYPE_INT)
            result[component_index] = (f32)tk.next_token.value.i;
        else
            result[component_index] = tk.next_token.value.f;

        SkipWhitespace(tk);

        if (PeekChar(tk) == ',')
        {
            NextChar(tk);
            SkipWhitespace(tk);
            component_index++;
            continue;
        }
    }

    tk.next_token = saved_token;
    tk.next_token.value.v4 = result;

    if (component_index == 0)
        EndToken(tk, TOKEN_TYPE_FLOAT);
    else if (component_index == 1)
        EndToken(tk, TOKEN_TYPE_VEC2);
    else if (component_index == 2)
        EndToken(tk, TOKEN_TYPE_VEC3);
    else if (component_index == 3)
        EndToken(tk, TOKEN_TYPE_VEC4);
    else
        return false;

    return true;
}

bool ReadColor(Tokenizer& tk)
{
    char c = PeekChar(tk);

    // Handle hex colors: #RRGGBB or #RRGGBBAA
    if (c == '#')
    {
        BeginToken(tk);

        // skip #
        NextChar(tk);

        while (isxdigit(PeekChar(tk)))
            NextChar(tk);

        EndToken(tk, TOKEN_TYPE_COLOR);

        // Extract hex to cstr
        char hex_str[16];
        Copy(hex_str, 16, tk.next_token.raw + 1, tk.next_token.length - 1);

        if (tk.next_token.length == 7) // #RRGGBB
        {
            u32 hex = (u32)strtoul(hex_str, nullptr, 16);
            tk.next_token.value.c.r = ((hex >> 16) & 0xFF) / 255.0f;
            tk.next_token.value.c.g = ((hex >> 8) & 0xFF) / 255.0f;
            tk.next_token.value.c.b = (hex & 0xFF) / 255.0f;
            tk.next_token.value.c.a = 1.0f;
            return true;
        }

        if (tk.next_token.length == 9) // #RRGGBBAA
        {
            unsigned int hex = (unsigned int)strtoul(hex_str, nullptr, 16);
            tk.next_token.value.c.r = ((hex >> 24) & 0xFF) / 255.0f;
            tk.next_token.value.c.g = ((hex >> 16) & 0xFF) / 255.0f;
            tk.next_token.value.c.b = ((hex >> 8) & 0xFF) / 255.0f;
            tk.next_token.value.c.a = (hex & 0xFF) / 255.0f;
            return true;
        }

        if (tk.next_token.length == 4) // #RGB shorthand
        {
            unsigned int hex = (unsigned int)strtoul(hex_str, nullptr, 16);
            tk.next_token.value.c.r = ((hex >> 8) & 0xF) / 15.0f;
            tk.next_token.value.c.g = ((hex >> 4) & 0xF) / 15.0f;
            tk.next_token.value.c.b = (hex & 0xF) / 15.0f;
            tk.next_token.value.c.a = 1.0f;
            return true;
        }

        tk.next_token.value.c = COLOR_WHITE;
        return true;
    }

    if (Equals(tk.input + tk.position, "rgba", 4, true))
    {
        BeginToken(tk);
        NextChar(tk);
        NextChar(tk);
        NextChar(tk);
        NextChar(tk);
        SkipWhitespace(tk);
        ReadVec(tk, false);

        Color color = COLOR_WHITE;
        if (tk.next_token.type == TOKEN_TYPE_VEC4)
        {
            color.r = tk.next_token.value.c.r / 255.0f;
            color.g = tk.next_token.value.c.g / 255.0f;
            color.b = tk.next_token.value.c.b / 255.0f;
            color.a = tk.next_token.value.c.a;
        }

        tk.next_token.type = TOKEN_TYPE_COLOR;
        tk.next_token.value.c = color;

        return true;
    }

    if (Equals(tk.input + tk.position, "rgb", 3, true))
    {
        BeginToken(tk);
        NextChar(tk);
        NextChar(tk);
        NextChar(tk);
        SkipWhitespace(tk);
        ReadVec(tk, false);

        Color color = COLOR_WHITE;
        if (tk.next_token.type == TOKEN_TYPE_VEC3)
        {
            color.r = tk.next_token.value.c.r / 255.0f;
            color.g = tk.next_token.value.c.g / 255.0f;
            color.b = tk.next_token.value.c.b / 255.0f;
        }

        tk.next_token.type = TOKEN_TYPE_COLOR;
        tk.next_token.value.c = color;

        return true;
    }

    // Check for predefined color names
    struct ColorName { const char* name; Color color; int name_len;};
    static ColorName predefined_colors[] = {
        {"black", {0.0f, 0.0f, 0.0f, 1.0f},},
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

    for (ColorName* color = predefined_colors; color->name != nullptr; color++)
    {
        if (color->name_len == 0)
            color->name_len = (int)Length(color->name);

        if (Equals(tk.input + tk.position, color->name, (u32)color->name_len, true))
        {
            BeginToken(tk);
            for (size_t i = 0; i < (u32)color->name_len; i++)
                NextChar(tk);
            EndToken(tk, TOKEN_TYPE_COLOR);
            tk.next_token.value.c = color->color;
            return true;
        }
    }

    return false;
}

bool ReadToken(Tokenizer& tk)
{
    // Save the value for reading since we are reading the next token
    tk.current_token = tk.next_token;

    SkipWhitespace(tk);

    if (!HasTokens(tk))
    {
        BeginToken(tk);
        EndToken(tk, TOKEN_TYPE_EOF);
        return false;
    }

    char c = PeekChar(tk);

    // Delimiter?
    if (IsDelimiter(c))
    {
        BeginToken(tk);
        NextChar(tk);
        EndToken(tk, TOKEN_TYPE_DELIMITER);
        return true;
    }

    // Quoted string.
    if (ReadQuotedString(tk))
        return true;

    // Read boolean
    if (ReadBool(tk))
        return true;

    // Color?
    if (ReadColor(tk))
        return true;

    // Read Vector
    if (ReadVec(tk))
        return true;

    // Number?
    if (ReadNumber(tk))
        return true;

    // Identifier?
    if (ReadIdentifier(tk))
        return true;

    NextChar(tk);
    EndToken(tk, TOKEN_TYPE_NONE);
    return true;
}

bool ExpectLine(Tokenizer& tk)
{
    // Rewind to before the next token
    tk.position = (u32)(tk.next_token.raw - tk.input);

    do
    {
        if (!HasTokens(tk))
            return false;

        while (HasTokens(tk) && tk.input[tk.position] != '\n')
            NextChar(tk);

        // Skip eol
        NextChar(tk);
        EndToken(tk, TOKEN_TYPE_STRING);

        while (tk.next_token.length > 0 && IsWhitespace(tk.next_token.raw[0]))
        {
            tk.next_token.length--;
            tk.next_token.raw++;
        }

        while (tk.next_token.length > 0 && IsWhitespace(tk.next_token.raw[tk.next_token.length - 1]))
            tk.next_token.length--;

    } while (tk.next_token.length == 0);

    ReadToken(tk);

    return tk.current_token.length > 0;
}

bool ExpectQuotedString(Tokenizer &tk)
{
    if (!Equals(tk.next_token, TOKEN_TYPE_STRING))
        return false;

    ReadToken(tk);

    return true;
}

bool ExpectInt(Tokenizer& tk, int* out_value)
{
    if (!Equals(tk.next_token, TOKEN_TYPE_INT))
        return false;

    if (out_value)
        *out_value = tk.next_token.value.i;

    ReadToken(tk);

    return true;
}

bool ExpectFloat(Tokenizer& tk, float* out_value)
{
    if (Equals(tk.next_token, TOKEN_TYPE_INT))
    {
        if (out_value)
            *out_value = (float)tk.next_token.value.i;

        ReadToken(tk);
        return true;
    }

    if (!Equals(tk.next_token, TOKEN_TYPE_FLOAT))
        return false;

    if (out_value)
        *out_value = tk.next_token.value.f;

    ReadToken(tk);

    return true;
}

bool ExpectIdentifier(Tokenizer& tk, const char* value)
{
    if (!Equals(tk.next_token, TOKEN_TYPE_IDENTIFIER))
        return false;

    bool result = !value || Equals(tk.next_token, value);

    if (result)
        ReadToken(tk);

    return result;
}

bool ExpectVec2(Tokenizer& tk, Vec2* out_value)
{
    if (!Equals(tk.next_token, TOKEN_TYPE_VEC2))
        return false;

    if (out_value)
        *out_value = tk.next_token.value.v2;

    ReadToken(tk);

    return true;
}

bool ExpectVec3(Tokenizer& tk, Vec3* out_value)
{
    if (!Equals(tk.next_token, TOKEN_TYPE_VEC3))
        return false;

    if (out_value)
        *out_value = tk.next_token.value.v3;

    ReadToken(tk);

    return true;
}

bool ExpectVec4(Tokenizer& tk, Vec4* out_value)
{
    if (!Equals(tk.next_token, TOKEN_TYPE_VEC4))
        return false;

    if (out_value)
        *out_value = tk.next_token.value.v4;

    ReadToken(tk);

    return true;
}

bool ExpectToken(Tokenizer& tk, Token* out_value) {
    if (Equals(tk.next_token, TOKEN_TYPE_EOF))
        return false;

    if (out_value)
        *out_value = tk.next_token;

    ReadToken(tk);
    return true;
}

bool ExpectColor(Tokenizer& tk, Color* out_value)
{
    if (!Equals(tk.next_token, TOKEN_TYPE_COLOR))
        return false;

    if (out_value)
        *out_value = tk.next_token.value.c;

    ReadToken(tk);

    return true;
}

bool ExpectDelimiter(Tokenizer& tk, char c)
{
    if (!Equals(tk.next_token, TOKEN_TYPE_DELIMITER))
        return false;

    bool result = c == *tk.next_token.raw && tk.next_token.length == 1;

    ReadToken(tk);

    return result;
}

void Init(Tokenizer& tk, const char* input)
{
    tk.input = input;
    tk.position = 0;
    tk.length = (u32)(input ? Length(input) : 0);
    tk.line = 1;
    tk.column = 1;

    ReadToken(tk);

    tk.current_token = tk.next_token;
}

#if defined(_STRING_)
std::string ToString(const Token& token) {
    char temp[4096];
    GetString(token, temp, sizeof(temp));
    return std::string(temp);
}
#endif
