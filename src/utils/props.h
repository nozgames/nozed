//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//
// @STL

#pragma once

#include <string>
#include <unordered_map>
#include <vector>

class Props
{
public:

    Props() = default;
    ~Props() = default;
    
    // Loading
    static Props* Load(Stream* stream);
    static Props* Load(const char* content, size_t content_length);

    // Clear all properties
    void Clear();
    void ClearGroup(const char* group);
    
    // @set
    void SetString(const char* group, const char* key, const char* value);
    void SetInt(const char* group, const char* key, int value);
    void SetFloat(const char* group, const char* key, float value);
    void SetVec3(const char* group, const char* key, Vec3 value);
    void SetVec2(const char* group, const char* key, const Vec2& value);
    void SetColor(const char* group, const char* key, Color value);

    // @get
    std::string GetString(const char* group, const char* key, const char* default_value) const;
    int GetInt(const char* group, const char* key, int default_value) const;
    float GetFloat(const char* group, const char* key, float default_value) const;
    bool GetBool(const char* group, const char* key, bool default_value) const;
    Vec3 GetVec3(const char* group, const char* key, Vec3 default_value) const;
    Vec2 GetVec2(const char* group, const char* key, const Vec2& default_value) const;
    Color GetColor(const char* group, const char* key, Color default_value) const;

    // @keys
    void AddKey(const char* group, const char* key);
    bool HasKey(const char* group, const char* key) const;
    std::vector<std::string> GetKeys(const char* group) const;

    // @groups
    bool HasGroup(const char* group) const;
    std::vector<std::string> GetGroups() const;

private:

    const std::unordered_map<std::string, std::string>& GetGroup(const char* group) const;
    std::unordered_map<std::string, std::string>& GetOrAddGroup(const char* group);

    std::unordered_map<std::string, std::unordered_map<std::string, std::string>> _properties;

    void RebuildKeyCache() const;
};

extern Props* LoadProps(const std::filesystem::path& path);
extern void SaveProps(Props* props, const std::filesystem::path& path);
