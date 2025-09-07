//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

#include <vector>

struct ShaderUniformBuffer;

struct ShaderReflectionResult
{
    std::vector<ShaderUniformBuffer> vertex_buffers;
    std::vector<ShaderUniformBuffer> fragment_buffers;
    int sampler_count;
};

ShaderReflectionResult ReflectShaderUniforms(
    const void* vertex_spirv,
    size_t vertex_size,
    const void* fragment_spirv,
    size_t fragment_size);