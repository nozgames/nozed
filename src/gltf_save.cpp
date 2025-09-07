//
//  MeshZ - Copyright(c) 2025 NoZ Games, LLC
//

#include "pch.h"

#define CGLTF_WRITE_IMPLEMENTATION
#include <cgltf_write.h>

bool SaveEditableMesh(const EditableMesh* mesh, const char* filename)
{
    if (!mesh || !filename)
        return false;

    // Create cgltf_data structure
    cgltf_data* data = (cgltf_data*)calloc(1, sizeof(cgltf_data));
    if (!data)
        return false;

    // Set required asset information
    data->asset.version = (char*)"2.0";
    data->asset.generator = (char*)"MeshZ";

    // Create buffers for vertex positions and indices
    size_t vertex_buffer_size = mesh->vertex_count * 3 * sizeof(float); // Vec3 positions (x,y,0)
    size_t index_buffer_size = mesh->triangle_count * 3 * sizeof(uint32_t); // Triangle indices
    size_t total_buffer_size = vertex_buffer_size + index_buffer_size;

    // Allocate buffer
    data->buffers_count = 1;
    data->buffers = (cgltf_buffer*)calloc(1, sizeof(cgltf_buffer));
    data->buffers[0].size = total_buffer_size;
    data->buffers[0].data = malloc(total_buffer_size);
    data->buffers[0].uri = nullptr; // No URI for GLB embedded data
    data->buffers[0].data_free_method = cgltf_data_free_method_memory_free;
    
    // For GLB format, we need to set the bin data at the cgltf_data level
    data->bin = data->buffers[0].data;
    data->bin_size = total_buffer_size;
    
    // Fill vertex data (positions as Vec3 with Z=0)
    float* vertex_data = (float*)data->buffers[0].data;
    for (int i = 0; i < mesh->vertex_count; i++)
    {
        vertex_data[i * 3 + 0] = mesh->vertices[i].position.x;
        vertex_data[i * 3 + 1] = mesh->vertices[i].position.y;
        vertex_data[i * 3 + 2] = 0.0f; // Z = 0 for 2D mesh in 3D space
    }
    
    // Fill index data
    uint32_t* index_data = (uint32_t*)((char*)data->buffers[0].data + vertex_buffer_size);
    for (int i = 0; i < mesh->triangle_count; i++)
    {
        index_data[i * 3 + 0] = mesh->triangles[i].v0;
        index_data[i * 3 + 1] = mesh->triangles[i].v1;
        index_data[i * 3 + 2] = mesh->triangles[i].v2;
    }

    // Create buffer views
    data->buffer_views_count = 2;
    data->buffer_views = (cgltf_buffer_view*)calloc(2, sizeof(cgltf_buffer_view));
    
    // Vertex buffer view
    data->buffer_views[0].buffer = &data->buffers[0];
    data->buffer_views[0].offset = 0;
    data->buffer_views[0].size = vertex_buffer_size;
    data->buffer_views[0].stride = 0; // Let accessor determine stride
    data->buffer_views[0].type = cgltf_buffer_view_type_vertices;
    
    // Index buffer view
    data->buffer_views[1].buffer = &data->buffers[0];
    data->buffer_views[1].offset = vertex_buffer_size;
    data->buffer_views[1].size = index_buffer_size;
    data->buffer_views[1].stride = 0; // Tightly packed indices
    data->buffer_views[1].type = cgltf_buffer_view_type_indices;

    // Create accessors
    data->accessors_count = 2;
    data->accessors = (cgltf_accessor*)calloc(2, sizeof(cgltf_accessor));
    
    // Position accessor
    data->accessors[0].buffer_view = &data->buffer_views[0];
    data->accessors[0].offset = 0;
    data->accessors[0].component_type = cgltf_component_type_r_32f;
    data->accessors[0].type = cgltf_type_vec3;
    data->accessors[0].count = mesh->vertex_count;
    data->accessors[0].stride = 3 * sizeof(float); // 12 bytes per vertex (3 floats)
    // Set min/max bounds for position data (required for validation)
    data->accessors[0].has_min = true;
    data->accessors[0].has_max = true;
    data->accessors[0].min[0] = -0.5f; data->accessors[0].min[1] = -0.5f; data->accessors[0].min[2] = 0.0f;
    data->accessors[0].max[0] = 0.5f; data->accessors[0].max[1] = 0.5f; data->accessors[0].max[2] = 0.0f;
    
    // Index accessor
    data->accessors[1].buffer_view = &data->buffer_views[1];
    data->accessors[1].offset = 0;
    data->accessors[1].component_type = cgltf_component_type_r_32u;
    data->accessors[1].type = cgltf_type_scalar;
    data->accessors[1].count = mesh->triangle_count * 3;
    data->accessors[1].stride = sizeof(uint32_t); // 4 bytes per index

    // Create mesh
    data->meshes_count = 1;
    data->meshes = (cgltf_mesh*)calloc(1, sizeof(cgltf_mesh));
    data->meshes[0].name = (char*)"EditableMesh";
    data->meshes[0].primitives_count = 1;
    data->meshes[0].primitives = (cgltf_primitive*)calloc(1, sizeof(cgltf_primitive));
    
    // Setup primitive
    cgltf_primitive* primitive = &data->meshes[0].primitives[0];
    primitive->type = cgltf_primitive_type_triangles;
    primitive->material = nullptr; // No material for now
    primitive->indices = &data->accessors[1];
    primitive->attributes_count = 1;
    primitive->attributes = (cgltf_attribute*)calloc(1, sizeof(cgltf_attribute));
    primitive->attributes[0].type = cgltf_attribute_type_position;
    primitive->attributes[0].data = &data->accessors[0];
    
    // Set the attribute name string (required for proper glTF output)
    size_t name_len = strlen("POSITION") + 1;
    primitive->attributes[0].name = (char*)malloc(name_len);
    strcpy(primitive->attributes[0].name, "POSITION");
    
    // Also set the accessor name
    primitive->attributes[0].data->name = (char*)malloc(name_len);
    strcpy(primitive->attributes[0].data->name, "POSITION");

    // Create node and scene
    data->nodes_count = 1;
    data->nodes = (cgltf_node*)calloc(1, sizeof(cgltf_node));
    data->nodes[0].mesh = &data->meshes[0];
    
    data->scenes_count = 1;
    data->scenes = (cgltf_scene*)calloc(1, sizeof(cgltf_scene));
    data->scenes[0].nodes_count = 1;
    data->scenes[0].nodes = (cgltf_node**)calloc(1, sizeof(cgltf_node*));
    data->scenes[0].nodes[0] = &data->nodes[0];
    data->scene = &data->scenes[0];

    // Validate the data before writing
    cgltf_result validate_result = cgltf_validate(data);
    if (validate_result != cgltf_result_success)
    {
        // Validation failed, cleanup and return false
        free(data->buffers[0].data);
        free(data->scenes[0].nodes);
        free(data->scenes);
        free(data->meshes[0].primitives[0].attributes[0].name);
        free(data->meshes[0].primitives[0].attributes[0].data->name);
        free(data->meshes[0].primitives[0].attributes);
        free(data->meshes[0].primitives);
        free(data->meshes);
        free(data->accessors);
        free(data->buffer_views);
        free(data->buffers);
        free(data->nodes);
        free(data);
        return false;
    }

    // Write to file
    cgltf_options options = {};
    options.type = cgltf_file_type_glb;
    
    cgltf_result result = cgltf_write_file(&options, filename, data);
    bool success = (result == cgltf_result_success);

    // Cleanup - free buffer data after writing
    if (data->buffers[0].data)
        free(data->buffers[0].data);
    free(data->scenes[0].nodes);
    free(data->scenes);
    free(data->meshes[0].primitives[0].attributes[0].name);
    free(data->meshes[0].primitives[0].attributes[0].data->name);
    free(data->meshes[0].primitives[0].attributes);
    free(data->meshes[0].primitives);
    free(data->meshes);
    free(data->accessors);
    free(data->buffer_views);
    free(data->buffers);
    free(data->nodes);
    free(data);

    return success;
}