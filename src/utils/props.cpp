//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

// @STL - This file uses STL since it's tools-only

#include "props.h"
#include "tokenizer.h"
#include <cstdio>

void Props::Clear()
{
    _properties.clear();
}

void Props::SetString(const char* group, const char* key, const char* value)
{
    assert(group && key && value);
    GetOrAddGroup(group)[key] = std::string(value);
}

void Props::SetInt(const char* group, const char* key, int value)
{
    char value_str[64];
    snprintf(value_str, sizeof(value_str), "%d", value);
    SetString(group, key, value_str);
}

void Props::SetFloat(const char* group, const char* key, float value)
{
    char value_str[32];
    snprintf(value_str, sizeof(value_str), "%.6f", value);
    SetString(group, key, value_str);
}

void Props::SetVec2(const char* group, const char* key, const Vec2& value)
{
    char value_str[128];
    snprintf(value_str, sizeof(value_str), "(%.6f,%.6f)", value.x, value.y);
    SetString(group, key, value_str);
}

void Props::SetVec3(const char* group, const char* key, Vec3 value)
{
    char value_str[128];
    snprintf(value_str, sizeof(value_str), "(%.6f,%.6f,%.6f)", value.x, value.y, value.z);
    SetString(group, key, value_str);
}

void Props::SetColor(const char* group, const char* key, Color value)
{
    char value_str[128];
    snprintf(value_str, sizeof(value_str), "rgba(%.0f,%.0f,%.0f,%.3f)",
             value.r * 255.0f, value.g * 255.0f, value.b * 255.0f, value.a);
    SetString(group, key, value_str);
}

void Props::AddKey(const char* group, const char* key)
{
    assert(group && key);
    GetOrAddGroup(group)[key] = std::string();
}

bool Props::HasKey(const char* group, const char* key) const
{
    assert(group && key);
    auto group_props = GetGroup(group);
    return group_props.find(key) != group_props.end();
}

std::string Props::GetString(const char* group, const char* key, const char* default_value) const
{
    assert(group && key);
    auto g = GetGroup(group);
    auto it = g.find(key);
    if (it == g.end())
        return default_value;

    return it->second;
}

int Props::GetInt(const char* group, const char* key, int default_value) const
{
    auto value = GetString(group, key, "");
    if (value.empty())
        return default_value;
    
    Tokenizer tok = {};
    Init(tok, value.c_str());

    int result = default_value;
    if (!ExpectInt(tok, &result))
        return default_value;

    return result;
}

float Props::GetFloat(const char* group, const char* key, float default_value) const
{
    auto value = GetString(group, key, "");
    if (value.empty())
        return default_value;

    Tokenizer tok = {};
    Init(tok, value.c_str());

    float result = default_value;
    if (!ExpectFloat(tok, &result))
        return default_value;

    return result;
}

bool Props::GetBool(const char* group, const char* key, bool default_value) const
{
    auto value = GetString(group, key, "");
    if (value.empty())
        return default_value;
    
    if (value == "true")
        return true;

    return false;
}

Vec3 Props::GetVec3(const char* group, const char* key, Vec3 default_value) const
{
    auto value = GetString(group, key, "");
    if (value.empty())
        return default_value;

    Tokenizer tok = {};
    Init(tok, value.c_str());

    Vec3 result = default_value;
    if (!ExpectVec3(tok, &result))
        return default_value;

    return result;
}

Vec2 Props::GetVec2(const char* group, const char* key, const Vec2& default_value) const
{
    auto value = GetString(group, key, "");
    if (value.empty())
        return default_value;

    Tokenizer tok = {};
    Init(tok, value.c_str());

    Vec2 result = default_value;
    if (!ExpectVec2(tok, &result))
        return default_value;

    return result;
}


Color Props::GetColor(const char* group, const char* key, Color default_value) const
{
    auto value = GetString(group, key, "");
    if (value.empty())
        return default_value;

    Tokenizer tok = {};
    Init(tok, value.c_str());

    Color result = default_value;
    if (!ExpectColor(tok, &result))
        return default_value;

    return result;
}

Props* Props::Load(Stream* stream)
{
    if (!stream) return nullptr;
    
    // Null-terminate the stream data
    SeekEnd(stream, 0);
    WriteU8(stream, 0);
    
    return Load((const char*)GetData(stream), GetSize(stream) - 1);
}

Props* Props::Load(const char* content, size_t content_length)
{
    (void)content_length;

    if (!content) return nullptr;

    auto props = new Props();
    Tokenizer tk = {};
    Init(tk, content);
    
    std::string group_name;
    
    while (!IsEOF(tk))
    {
        if (!ExpectLine(tk))
            break;

        std::string line_str = ::GetString(tk);

        if (line_str.size() == 0)
            continue;

        if (line_str[0] == '[' && line_str[line_str.length() - 1] == ']')
        {
            group_name = line_str.substr(1, line_str.size() - 2);
            continue;
        }

        Tokenizer tk_line;
        Init(tk_line, line_str.c_str());

        if (!ExpectIdentifier(tk_line))
            continue;

        std::string key = ::GetString(tk_line);
        if (key.size() == 0)
            continue;

        if (key == "position")
            key = "position";

        // value
        std::string value;
        if (ExpectDelimiter(tk_line, '='))
        {
            ExpectLine(tk_line);
            value = ::GetString(tk_line);
        }

        props->SetString(group_name.c_str(), key.c_str(), value.c_str());
    }
    
    return props;
}

const std::unordered_map<std::string, std::string>& Props::GetGroup(const char* group) const
{
    static std::unordered_map<std::string, std::string> empty = {};

    auto it = _properties.find(group);
    if (it == _properties.end())
        return empty;

    return it->second;
}

std::unordered_map<std::string, std::string>& Props::GetOrAddGroup(const char* group)
{
    auto& props = _properties[group];
    return props;
}


std::vector<std::string> Props::GetKeys(const char* group) const
{
    auto& props = GetGroup(group);
    std::vector<std::string> keys;
    keys.reserve(props.size());
    for (const auto& pair : props)
        keys.push_back(pair.first);
    return keys;
}

std::vector<std::string> Props::GetGroups() const
{
    std::vector<std::string> keys;
    keys.reserve(_properties.size());
    for (const auto& pair : _properties)
        keys.push_back(pair.first);

    return keys;
}

bool Props::HasGroup(const char* group) const
{
    return _properties.contains(group);
}

Props* LoadProps(const std::filesystem::path& path)
{
    Stream* stream = LoadStream(ALLOCATOR_DEFAULT, path);
    Props* props = nullptr;
    if (stream)
        props = Props::Load(stream);

    Free(stream);
    return props;
}

void SaveProps(Props* props, const std::filesystem::path& path)
{
    if (!props) return;
    
    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);
    
    // Get all groups and write them out in INI format
    auto groups = props->GetGroups();
    for (const auto& group_name : groups)
    {
        // Write group header
        WriteCSTR(stream, "[%s]\n", group_name.c_str());
        
        // Write all keys in this group
        auto keys = props->GetKeys(group_name.c_str());
        for (const auto& key : keys)
        {
            auto value = props->GetString(group_name.c_str(), key.c_str(), "");
            WriteCSTR(stream, "%s = %s\n", key.c_str(), value.c_str());
        }
        
        // Add blank line between groups for readability
        WriteCSTR(stream, "\n");
    }
    
    SaveStream(stream, path);
    Free(stream);
}
