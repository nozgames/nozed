//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "../../../src/internal.h"
#include "../utils/props.h"
#include <glslang_c_interface.h>

namespace fs = std::filesystem;

static std::string ProcessIncludes(const std::string& source, const fs::path& base_dir);
static std::vector<u32> CompileGLSLToSPIRV(const std::string& source, glslang_stage_t stage, const std::string& filename);

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

static void WriteCompiledShader(
    const std::string& vertex_shader,
    const std::string& geometry_shader,
    const std::string& fragment_shader,
    const std::string& original_source,
    const Props& meta,
    Stream* output_stream,
    const fs::path& include_dir,
    const std::string& source_path)
{
    // Preprocess includes and compile GLSL shaders to SPIR-V using glslang
    fs::path base_dir = include_dir;
    std::string processed_vertex = ProcessIncludes(vertex_shader, base_dir);
    std::string processed_fragment = ProcessIncludes(fragment_shader, base_dir);
    
    std::vector<u32> vertex_spirv = CompileGLSLToSPIRV(processed_vertex, GLSLANG_STAGE_VERTEX, source_path + ".vert");
    if (vertex_spirv.empty())
        throw std::runtime_error("Failed to compile vertex shader");

    // Compile geometry shader if present (ExtractStage already trims whitespace)
    std::vector<u32> geometry_spirv;
    if (!geometry_shader.empty())
    {
        // Validate that geometry shader has required layout declarations
        if (geometry_shader.find("layout(") == std::string::npos)
        {
            throw std::runtime_error("Geometry shader is missing required layout declarations (e.g., 'layout(triangles) in;' and 'layout(...) out;')");
        }
        
        std::string processed_geometry = ProcessIncludes(geometry_shader, base_dir);
        geometry_spirv = CompileGLSLToSPIRV(processed_geometry, GLSLANG_STAGE_GEOMETRY, source_path + ".geom");
        if (geometry_spirv.empty())
            throw std::runtime_error("Failed to compile geometry shader");
    }

    std::vector<u32> fragment_spirv = CompileGLSLToSPIRV(processed_fragment, GLSLANG_STAGE_FRAGMENT, source_path + ".frag");
    if (fragment_spirv.empty())
        throw std::runtime_error("Failed to compile fragment shader");

    // Write asset header
    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE_SHADER;
    header.version = 1;
    header.flags = 0;
    WriteAssetHeader(output_stream, &header);

    // Write bytecode sizes and data
    WriteU32(output_stream, (u32)(vertex_spirv.size() * sizeof(u32)));
    WriteBytes(output_stream, vertex_spirv.data(), vertex_spirv.size() * sizeof(u32));
    
    // Write geometry shader bytecode (0 size if no geometry shader)
    WriteU32(output_stream, (u32)(geometry_spirv.size() * sizeof(u32)));
    if (!geometry_spirv.empty())
        WriteBytes(output_stream, geometry_spirv.data(), geometry_spirv.size() * sizeof(u32));
    
    WriteU32(output_stream, (u32)(fragment_spirv.size() * sizeof(u32)));
    WriteBytes(output_stream, fragment_spirv.data(), fragment_spirv.size() * sizeof(u32));

    // Parse shader flags from meta file
    ShaderFlags flags = SHADER_FLAGS_NONE;
    if (meta.GetBool("shader", "blend", false))
        flags |= SHADER_FLAGS_BLEND;
    
    WriteU8(output_stream, (u8)flags);
}

void ImportShader(const fs::path& source_path, Stream* output_stream, Props* config, Props* meta)
{
    // Read source file
    std::ifstream file(source_path, std::ios::binary);
    if (!file.is_open())
        throw std::runtime_error("could not read file");

    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string source = buffer.str();
    file.close();

    // Extract each stage and write the shader
    std::string vertex_shader = ExtractStage(source, "VERTEX_SHADER");
    std::string geometry_shader = ExtractStage(source, "GEOMETRY_SHADER");
    std::string fragment_shader = ExtractStage(source, "FRAGMENT_SHADER");
    fs::path include_dir = source_path.parent_path();

    WriteCompiledShader(vertex_shader, geometry_shader, fragment_shader, source, *meta, output_stream, include_dir, source_path.string());
}

bool DoesShaderDependOn(const fs::path& source_path, const fs::path& dependency_path)
{
    std::string ext = dependency_path.extension().string();
    if (ext != ".glsl")
        return false;

    try
    {
        // Read source file
        std::ifstream file(source_path);
        if (!file.is_open())
            return false;
            
        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string source = buffer.str();
        file.close();
        
        // Simple check for #include references to the dependency file
        std::string dependency_name = dependency_path.filename().string();
        std::string include_pattern = "#include";
        
        size_t pos = 0;
        while ((pos = source.find(include_pattern, pos)) != std::string::npos)
        {
            size_t line_end = source.find('\n', pos);
            if (line_end != std::string::npos)
            {
                std::string line = source.substr(pos, line_end - pos);
                if (line.find(dependency_name) != std::string::npos)
                    return true;
            }
            pos += include_pattern.length();
        }
        
        return false;
    }
    catch (...)
    {
        return false;
    }
}

static const char* g_shader_extensions[] = { ".glsl", nullptr };

static AssetImporterTraits g_shader_importer_traits = {
    .type_name = "Shader",
    .signature = ASSET_SIGNATURE_SHADER,
    .file_extensions = g_shader_extensions,
    .import_func = ImportShader,
    .does_depend_on = DoesShaderDependOn
};

AssetImporterTraits* GetShaderImporterTraits()
{
    return &g_shader_importer_traits;
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
