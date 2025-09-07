//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "shader_reflect.h"
#include "../../src/internal.h"
#include <noz/noz.h>
// TODO: Implement proper reflection using glslang or SPIRV-Cross

static std::vector<ShaderUniformBuffer> ExtractUniformBuffers(spvc_context context, spvc_compiler compiler)
{
    std::vector<ShaderUniformBuffer> buffers;
    
    // Get shader resources
    spvc_resources resources;
    if (spvc_compiler_create_shader_resources(compiler, &resources) != SPVC_SUCCESS)
        return buffers;
    
    // Get uniform buffer resources
    const spvc_reflected_resource* ubos = nullptr;
    size_t ubo_count = 0;
    if (spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_UNIFORM_BUFFER, &ubos, &ubo_count) != SPVC_SUCCESS)
        return buffers;
    
    uint32_t current_offset = 0;
    
    // Define built-in buffer register slots that should be excluded
    // Use register constants to determine built-in vs user registers
    auto is_builtin_buffer = [&](uint32_t binding, uint32_t descriptor_set) -> bool {
        return (descriptor_set == 1 && binding < vertex_register_user0) ||    // space1: vertex built-ins
               (descriptor_set == 3 && binding < fragment_register_user0);    // space3: fragment built-ins
    };
    
    // Process each uniform buffer
    for (size_t i = 0; i < ubo_count; i++)
    {
        // Get binding and descriptor set information
        uint32_t binding = spvc_compiler_get_decoration(compiler, ubos[i].id, SpvDecorationBinding);
        uint32_t descriptor_set = spvc_compiler_get_decoration(compiler, ubos[i].id, SpvDecorationDescriptorSet);
        
        // Skip built-in buffers
        if (is_builtin_buffer(binding, descriptor_set))
            continue;
            
        spvc_type buffer_type = spvc_compiler_get_type_handle(compiler, ubos[i].base_type_id);
        size_t buffer_size = 0;
        if (spvc_compiler_get_declared_struct_size(compiler, buffer_type, &buffer_size) != SPVC_SUCCESS)
            continue;
        
        ShaderUniformBuffer buffer = {};
        buffer.size = static_cast<uint32_t>(buffer_size);
        buffer.offset = current_offset;
        
        buffers.push_back(buffer);
        current_offset += buffer.size;
    }
    
    return buffers;
}

ShaderReflectionResult ReflectShaderUniforms(
    const void* vertex_spirv,
    size_t vertex_size,
    const void* fragment_spirv,
    size_t fragment_size)
{
    ShaderReflectionResult result = {};
    
    // Create SPIRV-Cross context
    spvc_context context = nullptr;
    if (spvc_context_create(&context) != SPVC_SUCCESS)
        return result;
    
    // Reflect vertex shader
    if (vertex_spirv && vertex_size > 0)
    {
        spvc_parsed_ir parsed_ir = nullptr;
        if (spvc_context_parse_spirv(context, (const SpvId*)vertex_spirv, vertex_size / sizeof(SpvId), &parsed_ir) != SPVC_SUCCESS)
            goto cleanup;
        
        spvc_compiler compiler = nullptr;
        if (spvc_context_create_compiler(context, SPVC_BACKEND_NONE, parsed_ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler) != SPVC_SUCCESS)
            goto cleanup;
        
        result.vertex_buffers = ExtractUniformBuffers(context, compiler);
    }
    
    // Reflect fragment shader
    if (fragment_spirv && fragment_size > 0)
    {
        spvc_parsed_ir parsed_ir = nullptr;
        if (spvc_context_parse_spirv(context, (const SpvId*)fragment_spirv, fragment_size / sizeof(SpvId), &parsed_ir) != SPVC_SUCCESS)
            goto cleanup;
        
        spvc_compiler compiler = nullptr;
        if (spvc_context_create_compiler(context, SPVC_BACKEND_NONE, parsed_ir, SPVC_CAPTURE_MODE_TAKE_OWNERSHIP, &compiler) != SPVC_SUCCESS)
            goto cleanup;
        
        result.fragment_buffers = ExtractUniformBuffers(context, compiler);
        
        // Count samplers in fragment shader
        spvc_resources resources;
        if (spvc_compiler_create_shader_resources(compiler, &resources) == SPVC_SUCCESS)
        {
            const spvc_reflected_resource* samplers = nullptr;
            size_t sampler_count = 0;
            
            // First try combined sampled images (GLSL-style sampler2D)
            if (spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SAMPLED_IMAGE, &samplers, &sampler_count) == SPVC_SUCCESS && sampler_count > 0)
            {
                result.sampler_count = static_cast<int>(sampler_count);
            }
            // If no combined samplers, count separate images (HLSL-style Texture2D)
            else if (spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SEPARATE_IMAGE, &samplers, &sampler_count) == SPVC_SUCCESS && sampler_count > 0)
            {
                result.sampler_count = static_cast<int>(sampler_count);
            }
            // Fallback to separate samplers if that's all we have
            else if (spvc_resources_get_resource_list_for_type(resources, SPVC_RESOURCE_TYPE_SEPARATE_SAMPLERS, &samplers, &sampler_count) == SPVC_SUCCESS)
            {
                result.sampler_count = static_cast<int>(sampler_count);
            }
        }
    }
    
cleanup:
    spvc_context_destroy(context);
    return result;
}