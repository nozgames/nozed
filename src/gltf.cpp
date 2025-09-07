//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//
// @STL

#include "gltf.h"

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

static Color Vector4ToColor(float* v)
{
    if (!v)
        return COLOR_WHITE;

    return {v[0], v[1], v[2], v[3]};
}

static Vec2 Vector2ToVec2(float* vector2)
{
    if (!vector2)
        return Vec2(0.0f);

    return Vec2(vector2[0], vector2[1]);
}

static Vec3 Vector3ToVec3(float* vector3)
{
    if (!vector3)
        return VEC3_ZERO;

    return { vector3[0], vector3[1], vector3[2] };
}

static float QuaternionToYRotation(float* quaternion)
{
    if (!quaternion)
        return 0.0f;

    // quaternion format: [x, y, z, w]
    float x = quaternion[0];
    float y = quaternion[1];
    float z = quaternion[2];
    float w = quaternion[3];

    // Extract yaw (Y-axis rotation) from quaternion
    return atan2f(
        2.0f * (w * y + x * z),
        1.0f - 2.0f * (y * y + z * z));
}

// @file
bool GLTFLoader::open(const std::filesystem::path& file_path)
{
    Close();
    
    cgltf_options options = {};
    struct cgltf_data* gltf_data = nullptr;
    
    std::string path_str = file_path.string();
    cgltf_result result = cgltf_parse_file(&options, path_str.c_str(), &gltf_data);
    if (result != cgltf_result_success)
        return false;
    
    result = cgltf_load_buffers(&options, gltf_data, path_str.c_str());
    if (result != cgltf_result_success)
    {
        cgltf_free(gltf_data);
        return false;
    }
    
    data = gltf_data;
    path = file_path;
    
    return true;
}

void GLTFLoader::Close()
{
    if (data)
    {
        cgltf_free(data);
        data = nullptr;
    }
    path.clear();
}

// @class_methods
std::vector<GLTFBone> GLTFLoader::ReadBones()
{
    std::vector<GLTFBone> bones;
    if (!data || !data->nodes_count)
        return bones;
    
    // Find the root node (usually named "root")
    cgltf_node* root_node = nullptr;
    for (size_t i = 0; i < data->nodes_count; ++i)
    {
        cgltf_node* node = &data->nodes[i];
        if (node->name && std::string(node->name) == "root")
        {
            root_node = node;
            break;
        }
    }
    
    // If no root found, use first node with children
    if (!root_node)
    {
        for (size_t i = 0; i < data->nodes_count; ++i)
        {
            if (data->nodes[i].children_count > 0)
            {
                root_node = &data->nodes[i];
                break;
            }
        }
    }
    
    if (!root_node)
        return bones;
    
    // Recursively process bones starting from root
    ReadBone(root_node, bones, -1);
    
    // Update bone lengths and directions
    for (size_t i = 0; i < bones.size(); ++i)
    {
        GLTFBone& bone = bones[i];
        if (bone.parent_index >= 0)
        {
            GLTFBone& parent = bones[bone.parent_index];
            Vec3 direction = bone.position - parent.position;
            parent.length = Length(direction);
            if (parent.length > 0.0f)
                parent.direction = Normalize(direction);
        }
    }
    
    return bones;
}

void GLTFLoader::ReadBone(cgltf_node* node, std::vector<GLTFBone>& bones, int parent_index)
{
    if (!node || !node->name)
        return;
        
    std::string node_name = node->name;

    // Create the bone
    GLTFBone bone = {};
    bone.name = node_name;
    bone.index = static_cast<int>(bones.size());
    bone.parent_index = parent_index;
    bone.position = node->has_translation
        ? Vector3ToVec3(node->translation)
        : VEC3_ZERO;
    bone.rotation = Vec4{node->rotation[0], node->rotation[1], node->rotation[2], node->rotation[3]},
    bone.scale = node->has_scale
        ? Vector3ToVec3(node->scale)
        : VEC3_ONE;
    bone.local_to_world = TRS(bone.position, bone.rotation, bone.scale);

    if (parent_index >= 0)
        bone.local_to_world = bones[parent_index].local_to_world * bone.local_to_world;

    bone.world_to_local = Inverse(bone.world_to_local);

    bones.push_back(bone);
    int current_bone_index = static_cast<int>(bones.size() - 1);
    
    for (size_t i = 0; i < node->children_count; ++i)
        ReadBone(node->children[i], bones, current_bone_index);
}

GLTFMesh GLTFLoader::ReadMesh(const std::vector<GLTFBone>& bones)
{
    GLTFMesh mesh;
    if (!data || !data->meshes_count)
        return mesh;
        
    struct cgltf_mesh* cgltf_mesh = &data->meshes[0];
    if (!cgltf_mesh->primitives_count)
        return mesh;
        
    struct cgltf_primitive* primitive = &cgltf_mesh->primitives[0];
    
    // Read positions
    if (primitive->attributes)
    {
        for (size_t i = 0; i < primitive->attributes_count; ++i)
        {
            if (primitive->attributes[i].type == cgltf_attribute_type_position)
            {
                struct cgltf_accessor* accessor = primitive->attributes[i].data;
                if (accessor && accessor->count > 0)
                {
                    mesh.positions.resize(accessor->count);
                    u8* buffer_data = (u8*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset + accessor->offset;
                    
                    // Convert each position vector
                    float* positions_float = (float*)buffer_data;
                    for (size_t pos_idx = 0; pos_idx < accessor->count; ++pos_idx)
                    {
                        mesh.positions[pos_idx] = Vector3ToVec3(&positions_float[pos_idx * 3]);
                    }
                }
            }
            else if (primitive->attributes[i].type == cgltf_attribute_type_normal)
            {
                struct cgltf_accessor* accessor = primitive->attributes[i].data;
                if (accessor && accessor->count > 0)
                {
                    mesh.normals.resize(accessor->count);
                    u8* buffer_data = (u8*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset + accessor->offset;
                    
                    // Convert each normal vector
                    float* normals_float = (float*)buffer_data;
                    for (size_t norm_idx = 0; norm_idx < accessor->count; ++norm_idx)
                    {
                        mesh.normals[norm_idx] = Vector3ToVec3(&normals_float[norm_idx * 3]);
                    }
                }
            }
            else if (primitive->attributes[i].type == cgltf_attribute_type_texcoord)
            {
                struct cgltf_accessor* accessor = primitive->attributes[i].data;
                if (accessor && accessor->count > 0)
                {
                    mesh.uvs.resize(accessor->count);
                    u8* buffer_data = (u8*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset + accessor->offset;
                    
                    // Convert each UV coordinate
                    float* uvs_float = (float*)buffer_data;
                    for (size_t uv_idx = 0; uv_idx < accessor->count; ++uv_idx)
                    {
                        mesh.uvs[uv_idx] = Vector2ToVec2(&uvs_float[uv_idx * 2]);
                    }
                }
            }
            else if (primitive->attributes[i].type == cgltf_attribute_type_color)
            {
                cgltf_accessor* accessor = primitive->attributes[i].data;
                if (accessor && accessor->count > 0)
                {
                    mesh.colors.resize(accessor->count);
                    u8* buffer_data = (u8*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset + accessor->offset;
                    
                    // Handle different component types for color data
                    if (accessor->component_type == cgltf_component_type_r_32f)
                    {
                        // Float (GL_FLOAT)
                        float* color_float = (float*)buffer_data;
                        for (size_t color_index = 0; color_index < accessor->count; ++color_index)
                            mesh.colors[color_index] = Vector4ToColor(&color_float[color_index * 4]);
                    }
                    else if (accessor->component_type == cgltf_component_type_r_16u)
                    {
                        // Unsigned short (GL_UNSIGNED_SHORT) - normalized to [0,1]
                        uint16_t* color_ushort = (uint16_t*)buffer_data;
                        for (size_t color_index = 0; color_index < accessor->count; ++color_index)
                        {
                            float color_vec4[4];
                            for (int c = 0; c < 4; ++c)
                                color_vec4[c] = color_ushort[color_index * 4 + c] / 65535.0f;
                            mesh.colors[color_index] = Vector4ToColor(color_vec4);
                        }
                    }
                    else if (accessor->component_type == cgltf_component_type_r_8u)
                    {
                        // Unsigned byte (GL_UNSIGNED_BYTE) - normalized to [0,1]
                        u8* color_ubyte = (u8*)buffer_data;
                        for (size_t color_index = 0; color_index < accessor->count; ++color_index)
                        {
                            float color_vec4[4];
                            for (int c = 0; c < 4; ++c)
                                color_vec4[c] = color_ubyte[color_index * 4 + c] / 255.0f;
                            mesh.colors[color_index] = Vector4ToColor(color_vec4);
                        }
                    }
                    else
                    {
                        // Unsupported component type - use fallback
                        LogWarning("Unsupported color component type %d, using white", accessor->component_type);
                        for (size_t color_index = 0; color_index < accessor->count; ++color_index)
                        {
                            float white[4] = {1.0f, 1.0f, 1.0f, 1.0f};
                            mesh.colors[color_index] = Vector4ToColor(white);
                        }
                    }
                }
            }
            else if (primitive->attributes[i].type == cgltf_attribute_type_custom && strcmp(primitive->attributes[i].name,"_OUTLINE") == 0)
            {
                cgltf_accessor* accessor = primitive->attributes[i].data;
                if (!accessor || accessor->count <= 0)
                    continue;

                mesh.outlines.resize(accessor->count);
                u8* buffer_data = (u8*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset + accessor->offset;
                float* color_float = (float*)buffer_data;
                for (size_t index = 0; index < accessor->count; ++index)
                    mesh.outlines[index] = color_float[index];
            }
        }
    }

    if (mesh.outlines.size() == 0)
    {
        mesh.outlines.resize(mesh.positions.size());
        for (size_t i = 0; i < mesh.outlines.size(); ++i)
            mesh.outlines[i] = 1.0f;
    }
    
    // Read indices if present
    if (primitive->indices)
    {
        struct cgltf_accessor* accessor = primitive->indices;
        if (accessor && accessor->count > 0)
        {
            mesh.indices.resize(accessor->count);
            u8* buffer_data = (u8*)accessor->buffer_view->buffer->data + accessor->buffer_view->offset + accessor->offset;
            
            // Handle different index types
            if (accessor->component_type == cgltf_component_type_r_16u)
            {
                memcpy(mesh.indices.data(), buffer_data, accessor->count * sizeof(uint16_t));
            }
            else if (accessor->component_type == cgltf_component_type_r_32u)
            {
                // Convert from uint32 to uint16
                uint32_t* indices32 = (uint32_t*)buffer_data;
                for (size_t i = 0; i < accessor->count; ++i)
                {
                    mesh.indices[i] = static_cast<uint16_t>(indices32[i]);
                }
            }
        }
    }
    
    return mesh;
}

GLTFAnimation GLTFLoader::ReadAnimation(const std::vector<GLTFBone>& bones, const std::string& animation_name)
{
    GLTFAnimation animation;
    if (!data || !data->animations_count)
        return animation;
    
    // Find the animation by name
    struct cgltf_animation* cgltf_anim = nullptr;
    for (size_t i = 0; i < data->animations_count; ++i)
    {
        if (data->animations[i].name && animation_name == data->animations[i].name)
        {
            cgltf_anim = &data->animations[i];
            break;
        }
    }
    
    // If no name specified or not found, use first animation
    if (!cgltf_anim && data->animations_count > 0)
        cgltf_anim = &data->animations[0];
        
    if (!cgltf_anim || !cgltf_anim->channels_count)
        return animation;
    
    // Calculate frame count and stride
    animation.frame_count = 0;
    animation.frame_stride = static_cast<int>(bones.size() * 10); // position(3) + rotation(4) + scale(3)
    
    // Find maximum frame count across all channels
    for (size_t i = 0; i < cgltf_anim->channels_count; ++i)
    {
        cgltf_animation_channel* channel = &cgltf_anim->channels[i];
        if (channel->sampler && channel->sampler->input)
        {
            int frames = static_cast<int>(channel->sampler->input->count);
            animation.frame_count = std::max(animation.frame_count, frames);
        }
    }
    
    if (animation.frame_count == 0)
        return animation;
    
    // Allocate animation data
    size_t data_size = animation.frame_count * animation.frame_stride;
    animation.data.resize(data_size, 0.0f);
    
    // Process each animation channel
    for (size_t i = 0; i < cgltf_anim->channels_count; ++i)
    {
        cgltf_animation_channel* channel = &cgltf_anim->channels[i];
        if (!channel->sampler || !channel->target_node || !channel->target_node->name)
            continue;
            
        // Find the bone index for this channel's target node
        int bone_index = -1;
        for (size_t b = 0; b < bones.size(); ++b)
        {
            if (bones[b].name == channel->target_node->name)
            {
                bone_index = static_cast<int>(b);
                break;
            }
        }
        
        if (bone_index == -1)
            continue;
            
        cgltf_animation_sampler* sampler = channel->sampler;
        if (!sampler->input || !sampler->output)
            continue;
            
        u8* output_buffer =
            (u8*)sampler->output->buffer_view->buffer->data +
            sampler->output->buffer_view->offset +
            sampler->output->offset;
        
        for (size_t frame = 0; frame < sampler->input->count && frame < static_cast<size_t>(animation.frame_count); ++frame)
        {
            size_t frame_offset = frame * animation.frame_stride + bone_index * 10;
            
            if (channel->target_path == cgltf_animation_path_type_translation)
            {
                float* translation = (float*)(output_buffer + frame * 3 * sizeof(float));
                Vec3 pos = Vector3ToVec3(translation);
                animation.data[frame_offset + 0] = pos.x;
                animation.data[frame_offset + 1] = pos.y;
            }
            else if (channel->target_path == cgltf_animation_path_type_rotation)
            {
                f32* rotation = (float*)(output_buffer + frame * 4 * sizeof(float));
                f32 rot = QuaternionToYRotation(rotation);
                animation.data[frame_offset + 2] = rot;
            }
            else if (channel->target_path == cgltf_animation_path_type_scale)
            {
                float* scale = (float*)(output_buffer + frame * 3 * sizeof(float));
                Vec3 scl = Vector3ToVec3(scale);
                animation.data[frame_offset + 3] = scl.x;
                animation.data[frame_offset + 4] = scl.y;
            }
        }
    }
    
    return animation;
}
