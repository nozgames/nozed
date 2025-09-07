//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//
// @STL

#pragma once

#include <string>
#include <vector>
#include <filesystem>

// Forward declarations
struct cgltf_data;
struct cgltf_mesh;
struct cgltf_node;
struct cgltf_animation;
struct cgltf_animation_channel;
struct cgltf_skin;
struct cgltf_primitive;
struct cgltf_accessor;

// @types
struct GLTFBone
{
    std::string name;
    Mat4 world_to_local;
    Mat4 local_to_world;
    Vec3 position;
    Vec3 scale;
    Vec3 direction;
    int index;
    int parent_index;
    Vec4 rotation;
    float length;
};

struct GLTFAnimation
{
    int frame_count;
    int frame_stride;
    std::vector<void*> tracks;  // vector of animation_track_t
    std::vector<float> data;
};

struct GLTFMesh
{
    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> uvs;
    std::vector<Color> colors;
    std::vector<float> outlines;
    std::vector<uint32_t> bone_indices;
    std::vector<uint16_t> indices;
};

class GLTFLoader
{
    cgltf_data* data = nullptr;
    std::filesystem::path path;

public:
    GLTFLoader() = default;
    ~GLTFLoader() { Close(); }
    
    // Non-copyable
    GLTFLoader(const GLTFLoader&) = delete;
    GLTFLoader& operator=(const GLTFLoader&) = delete;
    
    // Movable
    GLTFLoader(GLTFLoader&& other) noexcept : data(other.data), path(std::move(other.path))
    {
        other.data = nullptr;
    }
    
    GLTFLoader& operator=(GLTFLoader&& other) noexcept
    {
        if (this != &other)
        {
            Close();
            data = other.data;
            path = std::move(other.path);
            other.data = nullptr;
        }
        return *this;
    }
    
    bool open(const std::filesystem::path& file_path);
    void Close();
    
    std::vector<GLTFBone> ReadBones();
    GLTFMesh ReadMesh(const std::vector<GLTFBone>& bones = {});
    GLTFAnimation ReadAnimation(const std::vector<GLTFBone>& bones, const std::string& animation_name);

private:

    void ReadBone(struct cgltf_node* node, std::vector<GLTFBone>& bones, int parent_index);
};