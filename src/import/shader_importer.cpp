//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "../../noz/src/internal.h"
#include "../utils/props.h"
#include "../editor.h"
#include <glslang_c_interface.h>
#include <sstream>

namespace fs = std::filesystem;

extern Editor g_editor;

static std::string ProcessIncludes(const std::string& source, const fs::path& base_dir);
static std::vector<u32> CompileGLSLToSPIRV(const std::string& source, glslang_stage_t stage, const std::string& filename);

// Convert Vulkan GLSL to desktop OpenGL 4.3 compatible GLSL
// - Changes #version 450 to #version 430 core
// - Removes set=X from layout qualifiers (Vulkan-specific)
// - Removes row_major qualifier, replaces with std140
// - Adds std140 to uniform blocks
static std::string ConvertToOpenGLSL(const std::string& source) {
    std::string result = source;

    // Remove #version directive - we'll add the correct one at the start
    std::regex version_pattern(R"(#version\s+\d+[^\n]*\n?)");
    result = std::regex_replace(result, version_pattern, "");

    // Remove set = X from inside layout() - Vulkan-specific, but keep binding
    std::regex set_pattern(R"(,?\s*set\s*=\s*\d+\s*,?)");
    result = std::regex_replace(result, set_pattern, ",");

    // Replace row_major with std140 for uniform blocks
    std::regex row_major_pattern(R"(\brow_major\b)");
    result = std::regex_replace(result, row_major_pattern, "std140");

    // Clean up any double commas or trailing/leading commas in layout()
    std::regex double_comma(R"(\s*,\s*,\s*)");
    result = std::regex_replace(result, double_comma, ", ");

    std::regex trailing_comma(R"(,\s*\))");
    result = std::regex_replace(result, trailing_comma, ")");

    std::regex leading_comma(R"(\(\s*,)");
    result = std::regex_replace(result, leading_comma, "(");

    // Clean up empty layout() declarations
    std::regex empty_layout(R"(layout\s*\(\s*\)\s*)");
    result = std::regex_replace(result, empty_layout, "");

    // Add std140 to uniform blocks that don't have it
    std::istringstream stream(result);
    std::ostringstream output;
    std::string line;

    std::regex uniform_block_no_layout(R"(^(\s*)uniform\s+(\w+)\s*\{)");
    std::regex layout_without_std140(R"(layout\s*\(([^)]*)\)\s*uniform\s+)");

    while (std::getline(stream, line)) {
        if (line.find("uniform") != std::string::npos &&
            line.find("sampler") == std::string::npos &&
            line.find("{") != std::string::npos) {
            if (line.find("layout") == std::string::npos) {
                // No layout, add layout(std140)
                line = std::regex_replace(line, uniform_block_no_layout, "$1layout(std140) uniform $2 {");
            } else if (line.find("std140") == std::string::npos) {
                // Has layout but no std140, add it
                line = std::regex_replace(line, layout_without_std140, "layout(std140, $1) uniform ");
            }
        }
        output << line << "\n";
    }
    result = output.str();

    // Final cleanup pass
    result = std::regex_replace(result, double_comma, ", ");
    result = std::regex_replace(result, trailing_comma, ")");
    result = std::regex_replace(result, leading_comma, "(");
    result = std::regex_replace(result, empty_layout, "");

    // Prepend OpenGL 4.3 core version
    std::string header = "#version 430 core\n\n";

    result = header + result;

    return result;
}


// Convert Vulkan GLSL to GLES 3.0 compatible GLSL for a single stage
// - Changes #version 450 to #version 300 es
// - Adds precision qualifiers required by GLES
// - Removes set=X and binding=Y from layout qualifiers (not supported in GLES 3.0)
// - Removes row_major qualifier, replaces with std140
// - Adds std140 to uniform blocks
// - Removes layout(location) from in/out (not fully supported in GLES 3.0)
static std::string ConvertToOpenGLES(const std::string& source) {
    std::string result = source;

    // Remove #version directive - we'll add the correct one at the start
    std::regex version_pattern(R"(#version\s+\d+[^\n]*\n?)");
    result = std::regex_replace(result, version_pattern, "");

    // Remove set = X and binding = Y from inside layout() - these are Vulkan-specific
    std::regex set_pattern(R"(,?\s*set\s*=\s*\d+\s*,?)");
    std::regex binding_pattern(R"(,?\s*binding\s*=\s*\d+\s*,?)");
    result = std::regex_replace(result, set_pattern, ",");
    result = std::regex_replace(result, binding_pattern, ",");

    // Replace row_major with std140 for uniform blocks
    std::regex row_major_pattern(R"(\brow_major\b)");
    result = std::regex_replace(result, row_major_pattern, "std140");

    // Remove layout(location = X) from in/out variables - GLES 3.0 has limited support
    // Vertex shader: location only valid on 'in', not 'out'
    // Fragment shader: location only valid on 'out', not 'in'
    // Simpler to just remove all location qualifiers and let the linker match by name
    std::regex location_pattern(R"(,?\s*location\s*=\s*\d+\s*,?)");
    result = std::regex_replace(result, location_pattern, ",");

    // Remove 'f' suffix from float literals - GLES 3.0 doesn't support it
    std::regex float_suffix_pattern(R"((\d+\.\d*|\d*\.\d+|\d+)[fF]\b)");
    result = std::regex_replace(result, float_suffix_pattern, "$1");

    // Clean up any double commas or trailing/leading commas in layout()
    std::regex double_comma(R"(\s*,\s*,\s*)");
    result = std::regex_replace(result, double_comma, ", ");

    std::regex trailing_comma(R"(,\s*\))");
    result = std::regex_replace(result, trailing_comma, ")");

    std::regex leading_comma(R"(\(\s*,)");
    result = std::regex_replace(result, leading_comma, "(");

    // Clean up empty layout() declarations
    std::regex empty_layout(R"(layout\s*\(\s*\)\s*)");
    result = std::regex_replace(result, empty_layout, "");

    // Add std140 to uniform blocks that don't have it
    std::istringstream stream(result);
    std::ostringstream output;
    std::string line;

    std::regex uniform_block_no_layout(R"(^(\s*)uniform\s+(\w+)\s*\{)");
    std::regex layout_without_std140(R"(layout\s*\(([^)]*)\)\s*uniform\s+)");

    while (std::getline(stream, line)) {
        if (line.find("uniform") != std::string::npos &&
            line.find("sampler") == std::string::npos &&
            line.find("{") != std::string::npos) {
            if (line.find("layout") == std::string::npos) {
                // No layout, add layout(std140)
                line = std::regex_replace(line, uniform_block_no_layout, "$1layout(std140) uniform $2 {");
            } else if (line.find("std140") == std::string::npos) {
                // Has layout but no std140, add it
                line = std::regex_replace(line, layout_without_std140, "layout(std140, $1) uniform ");
            }
        }
        output << line << "\n";
    }
    result = output.str();

    // Final cleanup pass
    result = std::regex_replace(result, double_comma, ", ");
    result = std::regex_replace(result, trailing_comma, ")");
    result = std::regex_replace(result, leading_comma, "(");
    result = std::regex_replace(result, empty_layout, "");

    // Prepend GLES 3.0 version and precision qualifiers
    std::string header = "#version 300 es\n"
                         "precision highp float;\n"
                         "precision highp int;\n\n";

    result = header + result;

    return result;
}

static std::string ExtractStage(const std::string& source, const std::string& stage)
{
    std::string result = source;

    // Convert custom stage directives to #ifdef blocks based on current stage
    // Format: //@ VERTEX ... //@ END, //@ GEOMETRY ... //@ END and //@ FRAGMENT ... //@ END

    // Use [\s\S] instead of . to match newlines (equivalent to dotall)
    std::regex vertex_pattern(R"(//@ VERTEX\s*\n([\s\S]*?)//@ END)");
    std::regex geometry_pattern(R"(//@ GEOMETRY\s*\n([\s\S]*?)//@ END)");
    std::regex fragment_pattern(R"(//@ FRAGMENT\s*\n([\s\S]*?)//@ END)");

    if (stage == "VERTEX_SHADER")
    {
        // Keep vertex shader blocks, remove other shader blocks
        result = std::regex_replace(result, vertex_pattern, "$1");
        result = std::regex_replace(result, geometry_pattern, "");
        result = std::regex_replace(result, fragment_pattern, "");
    }
    else if (stage == "GEOMETRY_SHADER")
    {
        // Check if geometry shader block actually exists
        if (std::regex_search(result, geometry_pattern))
        {
            // Keep geometry shader blocks, remove other shader blocks
            result = std::regex_replace(result, geometry_pattern, "$1");
            result = std::regex_replace(result, vertex_pattern, "");
            result = std::regex_replace(result, fragment_pattern, "");
        }
        else
        {
            // No geometry shader block found, return empty string
            result = "";
        }
    }
    else if (stage == "FRAGMENT_SHADER")
    {
        // Keep fragment shader blocks, remove other shader blocks
        result = std::regex_replace(result, fragment_pattern, "$1");
        result = std::regex_replace(result, vertex_pattern, "");
        result = std::regex_replace(result, geometry_pattern, "");
    }

    // Trim whitespace from the result
    result.erase(0, result.find_first_not_of(" \t\n\r"));
    result.erase(result.find_last_not_of(" \t\n\r") + 1);

    return result;
}

static void WriteGLSL(const fs::path& path, const std::string& vertex_source, const std::string& fragment_source, ShaderFlags flags, std::string (*convert_func)(const std::string&)) {
    std::string gl_vertex = convert_func(vertex_source);
    std::string gl_fragment = convert_func(fragment_source);

    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);

    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE;
    header.type = ASSET_TYPE_SHADER;
    header.version = 2;
    header.flags = 0;
    WriteAssetHeader(stream, &header);
    WriteU32(stream, (u32)gl_vertex.size());
    WriteBytes(stream, gl_vertex.data(), (u32)gl_vertex.size());
    WriteU32(stream, (u32)gl_fragment.size());
    WriteBytes(stream, gl_fragment.data(), (u32)gl_fragment.size());
    WriteU8(stream, (u8)flags);
    SaveStream(stream, path);
}

static void WriteSPIRV(
    const fs::path& path,
    const std::string& vertex_shader,
    const std::string& fragment_shader,
    const fs::path& include_dir,
    const std::string& source_path,
    ShaderFlags flags
    ) {
    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);

    // Preprocess includes and compile GLSL shaders to SPIR-V using glslang
    fs::path base_dir = include_dir;
    std::string processed_vertex = ProcessIncludes(vertex_shader, base_dir);
    std::string processed_fragment = ProcessIncludes(fragment_shader, base_dir);
    std::string processed_geometry;

    std::vector<u32> vertex_spirv = CompileGLSLToSPIRV(processed_vertex, GLSLANG_STAGE_VERTEX, source_path + ".vert");
    if (vertex_spirv.empty())
        throw std::runtime_error("Failed to compile vertex shader");

    std::vector<u32> fragment_spirv = CompileGLSLToSPIRV(processed_fragment, GLSLANG_STAGE_FRAGMENT, source_path + ".frag");
    if (fragment_spirv.empty())
        throw std::runtime_error("Failed to compile fragment shader");

    // Write asset header (version 2 includes embedded GLSL)
    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE;
    header.type = ASSET_TYPE_SHADER;
    header.version = 2;
    header.flags = 0;
    WriteAssetHeader(stream, &header);

    WriteU32(stream, (u32)(vertex_spirv.size() * sizeof(u32)));
    WriteBytes(stream, vertex_spirv.data(), (u32)(vertex_spirv.size() * sizeof(u32)));
    WriteU32(stream, (u32)(fragment_spirv.size() * sizeof(u32)));
    WriteBytes(stream, fragment_spirv.data(), (u32)(fragment_spirv.size() * sizeof(u32)));
    WriteU8(stream, (u8)flags);
    SaveStream(stream, path.string());
}

static void ImportShader(AssetData* a, const std::filesystem::path& path, Props* config, Props* meta) {
    (void)config;

    // Read source file
    std::ifstream file(a->path, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("could not read file");

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    file.close();

    // Extract each stage and write the shader
    std::string vertex_shader = ExtractStage(source, "VERTEX_SHADER");
    std::string fragment_shader = ExtractStage(source, "FRAGMENT_SHADER");
    fs::path include_dir = fs::path(a->path).parent_path();

    // Parse shader flags from meta file
    ShaderFlags flags = SHADER_FLAGS_NONE;
    if (meta->GetBool("shader", "blend", false))
        flags |= SHADER_FLAGS_BLEND;

    if (meta->GetBool("shader", "depth", false))
        flags |= SHADER_FLAGS_DEPTH;

    if (meta->GetBool("shader", "depth_less", false))
        flags |= SHADER_FLAGS_DEPTH_LESS;

    if (meta->GetBool("shader", "postproc", false))
        flags |= SHADER_FLAGS_POSTPROCESS;

    if (meta->GetBool("shader", "composite", false))
        flags |= SHADER_FLAGS_UI_COMPOSITE;

    if (meta->GetBool("shader", "premultiplied", false))
        flags |= SHADER_FLAGS_PREMULTIPLIED_ALPHA;


    try {
        WriteSPIRV(path, vertex_shader, fragment_shader, include_dir, a->path, flags);
        WriteGLSL(path.string() + ".glsl", vertex_shader, fragment_shader, flags, ConvertToOpenGLSL);
        WriteGLSL(path.string() + ".gles", vertex_shader, fragment_shader, flags, ConvertToOpenGLES);

    } catch (const std::runtime_error& e) {
        LogError(e.what());
    }
}

static std::vector<u32> CompileGLSLToSPIRV(const std::string& source, glslang_stage_t stage, const std::string& filename)
{
    // Initialize glslang if not already done
    static bool initialized = false;
    if (!initialized) {
        glslang_initialize_process();
        initialized = true;
    }

    // Create default resource limits
    static glslang_resource_t resource = {
        .max_lights = 32,
        .max_clip_planes = 6,
        .max_texture_units = 32,
        .max_texture_coords = 32,
        .max_vertex_attribs = 64,
        .max_vertex_uniform_components = 4096,
        .max_varying_floats = 64,
        .max_vertex_texture_image_units = 32,
        .max_combined_texture_image_units = 80,
        .max_texture_image_units = 32,
        .max_fragment_uniform_components = 4096,
        .max_draw_buffers = 32,
        .max_vertex_uniform_vectors = 128,
        .max_varying_vectors = 8,
        .max_fragment_uniform_vectors = 16,
        .max_vertex_output_vectors = 16,
        .max_fragment_input_vectors = 15,
        .min_program_texel_offset = -8,
        .max_program_texel_offset = 7,
        .max_clip_distances = 8,
        .max_compute_work_group_count_x = 65535,
        .max_compute_work_group_count_y = 65535,
        .max_compute_work_group_count_z = 65535,
        .max_compute_work_group_size_x = 1024,
        .max_compute_work_group_size_y = 1024,
        .max_compute_work_group_size_z = 64,
        .max_compute_uniform_components = 1024,
        .max_compute_texture_image_units = 16,
        .max_compute_image_uniforms = 8,
        .max_compute_atomic_counters = 8,
        .max_compute_atomic_counter_buffers = 1,
        .max_varying_components = 60,
        .max_vertex_output_components = 64,
        .max_geometry_input_components = 64,
        .max_geometry_output_components = 128,
        .max_fragment_input_components = 128,
        .max_image_units = 8,
        .max_combined_image_units_and_fragment_outputs = 8,
        .max_combined_shader_output_resources = 8,
        .max_image_samples = 0,
        .max_vertex_image_uniforms = 0,
        .max_tess_control_image_uniforms = 0,
        .max_tess_evaluation_image_uniforms = 0,
        .max_geometry_image_uniforms = 0,
        .max_fragment_image_uniforms = 8,
        .max_combined_image_uniforms = 8,
        .max_geometry_texture_image_units = 16,
        .max_geometry_output_vertices = 256,
        .max_geometry_total_output_components = 1024,
        .max_geometry_uniform_components = 1024,
        .max_geometry_varying_components = 64,
        .max_tess_control_input_components = 128,
        .max_tess_control_output_components = 128,
        .max_tess_control_texture_image_units = 16,
        .max_tess_control_uniform_components = 1024,
        .max_tess_control_total_output_components = 4096,
        .max_tess_evaluation_input_components = 128,
        .max_tess_evaluation_output_components = 128,
        .max_tess_evaluation_texture_image_units = 16,
        .max_tess_evaluation_uniform_components = 1024,
        .max_tess_patch_components = 120,
        .max_patch_vertices = 32,
        .max_tess_gen_level = 64,
        .max_viewports = 16,
        .max_vertex_atomic_counters = 0,
        .max_tess_control_atomic_counters = 0,
        .max_tess_evaluation_atomic_counters = 0,
        .max_geometry_atomic_counters = 0,
        .max_fragment_atomic_counters = 8,
        .max_combined_atomic_counters = 8,
        .max_atomic_counter_bindings = 1,
        .max_vertex_atomic_counter_buffers = 0,
        .max_tess_control_atomic_counter_buffers = 0,
        .max_tess_evaluation_atomic_counter_buffers = 0,
        .max_geometry_atomic_counter_buffers = 0,
        .max_fragment_atomic_counter_buffers = 1,
        .max_combined_atomic_counter_buffers = 1,
        .max_atomic_counter_buffer_size = 16384,
        .max_transform_feedback_buffers = 4,
        .max_transform_feedback_interleaved_components = 64,
        .max_cull_distances = 8,
        .max_combined_clip_and_cull_distances = 8,
        .max_samples = 4,
        .max_mesh_output_vertices_nv = 256,
        .max_mesh_output_primitives_nv = 512,
        .max_mesh_work_group_size_x_nv = 32,
        .max_mesh_work_group_size_y_nv = 1,
        .max_mesh_work_group_size_z_nv = 1,
        .max_task_work_group_size_x_nv = 32,
        .max_task_work_group_size_y_nv = 1,
        .max_task_work_group_size_z_nv = 1,
        .max_mesh_view_count_nv = 4,
        .max_dual_source_draw_buffers_ext = 1,
        .limits = {
            .non_inductive_for_loops = true,
            .while_loops = true,
            .do_while_loops = true,
            .general_uniform_indexing = true,
            .general_attribute_matrix_vector_indexing = true,
            .general_varying_indexing = true,
            .general_sampler_indexing = true,
            .general_variable_indexing = true,
            .general_constant_matrix_vector_indexing = true,
        }
    };

    // Create input
    glslang_input_t input = {
        .language = GLSLANG_SOURCE_GLSL,
        .stage = stage,
        .client = GLSLANG_CLIENT_VULKAN,
        .client_version = GLSLANG_TARGET_VULKAN_1_0,
        .target_language = GLSLANG_TARGET_SPV,
        .target_language_version = GLSLANG_TARGET_SPV_1_0,
        .code = source.c_str(),
        .default_version = 450,
        .default_profile = GLSLANG_NO_PROFILE,
        .force_default_version_and_profile = false,
        .forward_compatible = false,
        .messages = GLSLANG_MSG_DEFAULT_BIT,
        .resource = &resource,
    };

    // Create shader and parse
    glslang_shader_t* shader = glslang_shader_create(&input);
    if (!glslang_shader_preprocess(shader, &input))
        throw std::runtime_error(std::string(glslang_shader_get_info_log(shader)));

    if (!glslang_shader_parse(shader, &input))
        throw std::runtime_error(std::string(glslang_shader_get_info_log(shader)));

    // Create program and link
    glslang_program_t* program = glslang_program_create();
    glslang_program_add_shader(program, shader);

    if (!glslang_program_link(program, GLSLANG_MSG_SPV_RULES_BIT | GLSLANG_MSG_VULKAN_RULES_BIT)) {
        std::string error_msg = std::string(glslang_program_get_info_log(program));
        glslang_program_delete(program);
        glslang_shader_delete(shader);
        throw std::runtime_error(error_msg);
    }

    // Generate SPIR-V
    glslang_program_SPIRV_generate(program, stage);
    
    if (glslang_program_SPIRV_get_messages(program)) {
        printf("SPIR-V generation messages for %s:\n%s\n", filename.c_str(), glslang_program_SPIRV_get_messages(program));
    }

    // Get SPIR-V data
    size_t spirv_size = glslang_program_SPIRV_get_size(program);
    const u32* spirv_data = glslang_program_SPIRV_get_ptr(program);
    
    std::vector spirv(spirv_data, spirv_data + spirv_size);

    // Cleanup
    glslang_program_delete(program);
    glslang_shader_delete(shader);

    return spirv;
}

static std::string ProcessIncludes(const std::string& source, const fs::path& base_dir)
{
    std::string result;
    result.reserve(source.size() * 2); // Reserve some space
    
    std::istringstream input(source);
    std::string line;
    
    while (std::getline(input, line))
    {
        // Trim whitespace from the beginning
        size_t start = line.find_first_not_of(" \t");
        if (start != std::string::npos)
        {
            // Check if this is an #include line
            if (line.substr(start, 8) == "#include")
            {
                // Find the filename in quotes
                size_t quote1 = line.find('"', start + 8);
                if (quote1 != std::string::npos)
                {
                    size_t quote2 = line.find('"', quote1 + 1);
                    if (quote2 != std::string::npos)
                    {
                        std::string filename = line.substr(quote1 + 1, quote2 - quote1 - 1);
                        fs::path include_path = base_dir / filename;
                        
                        // Read the include file
                        std::ifstream include_file(include_path);
                        if (include_file.is_open())
                        {
                            std::string include_content((std::istreambuf_iterator(include_file)),
                                                       std::istreambuf_iterator<char>());
                            
                            // Recursively process includes in the included file
                            std::string processed_include = ProcessIncludes(include_content, include_path.parent_path());
                            
                            result += processed_include;
                            result += "\n";
                        }
                        else
                        {
                            std::string clean_path = include_path.string();
                            std::replace(clean_path.begin(), clean_path.end(), '\\', '/');
                            throw std::runtime_error("Could not open include file: " + clean_path);
                        }
                        continue; // Skip adding the original #include line
                    }
                }
            }
        }
        
        // Add the original line if it's not an #include
        result += line;
        result += "\n";
    }
    
    return result;
}

AssetImporter GetShaderImporter()
{
    return {
        .type = ASSET_TYPE_SHADER,
        .ext = ".glsl",
        .import_func = ImportShader
    };
}
