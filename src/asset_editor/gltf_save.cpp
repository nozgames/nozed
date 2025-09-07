//
//  MeshZ - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"

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

    // Generate separate vertices for each triangle (3 verts per triangle)
    int total_vertices = mesh->triangle_count * 3;
    size_t vertex_buffer_size = total_vertices * 3 * sizeof(float); // Vec3 positions (x,y,0)
    size_t normal_buffer_size = total_vertices * 3 * sizeof(float); // Vec3 normals
    size_t uv_buffer_size = total_vertices * 2 * sizeof(float); // Vec2 UVs (u,v)
    size_t index_buffer_size = total_vertices * sizeof(uint32_t); // Sequential indices
    size_t total_buffer_size = vertex_buffer_size + normal_buffer_size + uv_buffer_size + index_buffer_size;

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
    
    // Fill vertex data (separate vertex for each triangle vertex)
    float* vertex_data = (float*)data->buffers[0].data;
    for (int i = 0; i < mesh->triangle_count; i++)
    {
        const EditableTriangle& tri = mesh->triangles[i];
        
        // Vertex 0 of triangle
        vertex_data[(i * 3 + 0) * 3 + 0] = mesh->vertices[tri.v0].position.x;
        vertex_data[(i * 3 + 0) * 3 + 1] = mesh->vertices[tri.v0].position.y;
        vertex_data[(i * 3 + 0) * 3 + 2] = 0.0f;
        
        // Vertex 1 of triangle
        vertex_data[(i * 3 + 1) * 3 + 0] = mesh->vertices[tri.v1].position.x;
        vertex_data[(i * 3 + 1) * 3 + 1] = mesh->vertices[tri.v1].position.y;
        vertex_data[(i * 3 + 1) * 3 + 2] = 0.0f;
        
        // Vertex 2 of triangle
        vertex_data[(i * 3 + 2) * 3 + 0] = mesh->vertices[tri.v2].position.x;
        vertex_data[(i * 3 + 2) * 3 + 1] = mesh->vertices[tri.v2].position.y;
        vertex_data[(i * 3 + 2) * 3 + 2] = 0.0f;
    }
    
    // Fill normal data (calculate normal from triangle vertices)
    float* normal_data = (float*)((char*)data->buffers[0].data + vertex_buffer_size);
    for (int i = 0; i < mesh->triangle_count; i++)
    {
        const EditableTriangle& tri = mesh->triangles[i];
        
        // Get triangle vertex positions
        Vec2 v0_pos = mesh->vertices[tri.v0].position;
        Vec2 v1_pos = mesh->vertices[tri.v1].position;
        Vec2 v2_pos = mesh->vertices[tri.v2].position;
        
        // Convert to Vec3 (z=0 for 2D mesh)
        Vec3 p0 = {v0_pos.x, v0_pos.y, 0.0f};
        Vec3 p1 = {v1_pos.x, v1_pos.y, 0.0f};
        Vec3 p2 = {v2_pos.x, v2_pos.y, 0.0f};
        
        // Calculate triangle normal using cross product
        Vec3 edge1 = {p1.x - p0.x, p1.y - p0.y, p1.z - p0.z};
        Vec3 edge2 = {p2.x - p0.x, p2.y - p0.y, p2.z - p0.z};
        
        // Cross product: edge1 × edge2
        Vec3 normal = {
            edge1.y * edge2.z - edge1.z * edge2.y,
            edge1.z * edge2.x - edge1.x * edge2.z,
            edge1.x * edge2.y - edge1.y * edge2.x
        };
        
        // Normalize the normal (for 2D mesh, this should be (0,0,1) or (0,0,-1))
        float length = sqrtf(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
        if (length > 0.0f)
        {
            normal.x /= length;
            normal.y /= length;
            normal.z /= length;
        }
        else
        {
            // Default normal for degenerate triangle
            normal = {0.0f, 0.0f, 1.0f};
        }
        
        // Use same normal for all 3 vertices of this triangle
        for (int v = 0; v < 3; v++)
        {
            normal_data[(i * 3 + v) * 3 + 0] = normal.x;
            normal_data[(i * 3 + v) * 3 + 1] = normal.y;
            normal_data[(i * 3 + v) * 3 + 2] = normal.z;
        }
    }
    
    // Fill UV data (same UV for all 3 vertices of each triangle)
    float* uv_data = (float*)((char*)data->buffers[0].data + vertex_buffer_size + normal_buffer_size);
    for (int i = 0; i < mesh->triangle_count; i++)
    {
        const EditableTriangle& tri = mesh->triangles[i];
        
        // Convert Vec2Int color to UV coordinates using ColorUV
        Vec2 uv_color = ColorUV(tri.color.x, tri.color.y);
        
        // Debug: Print first few triangles being saved
        if (i < 3)
        {
            printf("SAVE Triangle %d: Color(%d, %d) -> UV(%.3f, %.3f)\n", i, tri.color.x, tri.color.y, uv_color.x, uv_color.y);
        }
        
        // Set same UV for all 3 vertices of this triangle (Vec2, not Vec3)
        for (int v = 0; v < 3; v++)
        {
            uv_data[(i * 3 + v) * 2 + 0] = uv_color.x;
            uv_data[(i * 3 + v) * 2 + 1] = uv_color.y;
        }
    }
    
    // Fill index data (sequential indices since each vertex is unique)
    uint32_t* index_data = (uint32_t*)((char*)data->buffers[0].data + vertex_buffer_size + normal_buffer_size + uv_buffer_size);
    for (int i = 0; i < total_vertices; i++)
    {
        index_data[i] = i;
    }

    // Create buffer views
    data->buffer_views_count = 4;
    data->buffer_views = (cgltf_buffer_view*)calloc(4, sizeof(cgltf_buffer_view));
    
    // Vertex buffer view
    data->buffer_views[0].buffer = &data->buffers[0];
    data->buffer_views[0].offset = 0;
    data->buffer_views[0].size = vertex_buffer_size;
    data->buffer_views[0].stride = 0; // Let accessor determine stride
    data->buffer_views[0].type = cgltf_buffer_view_type_vertices;
    
    // Normal buffer view
    data->buffer_views[1].buffer = &data->buffers[0];
    data->buffer_views[1].offset = vertex_buffer_size;
    data->buffer_views[1].size = normal_buffer_size;
    data->buffer_views[1].stride = 0; // Let accessor determine stride
    data->buffer_views[1].type = cgltf_buffer_view_type_vertices;
    
    // UV buffer view
    data->buffer_views[2].buffer = &data->buffers[0];
    data->buffer_views[2].offset = vertex_buffer_size + normal_buffer_size;
    data->buffer_views[2].size = uv_buffer_size;
    data->buffer_views[2].stride = 0; // Let accessor determine stride
    data->buffer_views[2].type = cgltf_buffer_view_type_vertices;
    
    // Index buffer view
    data->buffer_views[3].buffer = &data->buffers[0];
    data->buffer_views[3].offset = vertex_buffer_size + normal_buffer_size + uv_buffer_size;
    data->buffer_views[3].size = index_buffer_size;
    data->buffer_views[3].stride = 0; // Tightly packed indices
    data->buffer_views[3].type = cgltf_buffer_view_type_indices;

    // Create accessors
    data->accessors_count = 4;
    data->accessors = (cgltf_accessor*)calloc(4, sizeof(cgltf_accessor));
    
    // Position accessor
    data->accessors[0].buffer_view = &data->buffer_views[0];
    data->accessors[0].offset = 0;
    data->accessors[0].component_type = cgltf_component_type_r_32f;
    data->accessors[0].type = cgltf_type_vec3;
    data->accessors[0].count = total_vertices;
    data->accessors[0].stride = 3 * sizeof(float); // 12 bytes per vertex (3 floats)
    // Set min/max bounds for position data (required for validation)
    data->accessors[0].has_min = true;
    data->accessors[0].has_max = true;
    data->accessors[0].min[0] = -0.5f; data->accessors[0].min[1] = -0.5f; data->accessors[0].min[2] = 0.0f;
    data->accessors[0].max[0] = 0.5f; data->accessors[0].max[1] = 0.5f; data->accessors[0].max[2] = 0.0f;
    
    // Normal accessor
    data->accessors[1].buffer_view = &data->buffer_views[1];
    data->accessors[1].offset = 0;
    data->accessors[1].component_type = cgltf_component_type_r_32f;
    data->accessors[1].type = cgltf_type_vec3;
    data->accessors[1].count = total_vertices;
    data->accessors[1].stride = 3 * sizeof(float); // 12 bytes per normal (3 floats)
    
    // UV accessor
    data->accessors[2].buffer_view = &data->buffer_views[2];
    data->accessors[2].offset = 0;
    data->accessors[2].component_type = cgltf_component_type_r_32f;
    data->accessors[2].type = cgltf_type_vec2;
    data->accessors[2].count = total_vertices;
    data->accessors[2].stride = 2 * sizeof(float); // 8 bytes per UV (2 floats)
    
    // Index accessor
    data->accessors[3].buffer_view = &data->buffer_views[3];
    data->accessors[3].offset = 0;
    data->accessors[3].component_type = cgltf_component_type_r_32u;
    data->accessors[3].type = cgltf_type_scalar;
    data->accessors[3].count = total_vertices;
    data->accessors[3].stride = sizeof(uint32_t); // 4 bytes per index

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
    primitive->indices = &data->accessors[3]; // Index accessor is now at index 3
    primitive->attributes_count = 3; // Position, Normal, and UV
    primitive->attributes = (cgltf_attribute*)calloc(3, sizeof(cgltf_attribute));
    
    // Position attribute
    primitive->attributes[0].type = cgltf_attribute_type_position;
    primitive->attributes[0].data = &data->accessors[0];
    size_t pos_name_len = strlen("POSITION") + 1;
    primitive->attributes[0].name = (char*)malloc(pos_name_len);
    strcpy(primitive->attributes[0].name, "POSITION");
    primitive->attributes[0].data->name = (char*)malloc(pos_name_len);
    strcpy(primitive->attributes[0].data->name, "POSITION");
    
    // Normal attribute
    primitive->attributes[1].type = cgltf_attribute_type_normal;
    primitive->attributes[1].data = &data->accessors[1];
    size_t normal_name_len = strlen("NORMAL") + 1;
    primitive->attributes[1].name = (char*)malloc(normal_name_len);
    strcpy(primitive->attributes[1].name, "NORMAL");
    primitive->attributes[1].data->name = (char*)malloc(normal_name_len);
    strcpy(primitive->attributes[1].data->name, "NORMAL");
    
    // UV attribute
    primitive->attributes[2].type = cgltf_attribute_type_texcoord;
    primitive->attributes[2].data = &data->accessors[2];
    size_t uv_name_len = strlen("TEXCOORD_0") + 1;
    primitive->attributes[2].name = (char*)malloc(uv_name_len);
    strcpy(primitive->attributes[2].name, "TEXCOORD_0");
    primitive->attributes[2].data->name = (char*)malloc(uv_name_len);
    strcpy(primitive->attributes[2].data->name, "TEXCOORD_0");

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
        free(data->meshes[0].primitives[0].attributes[1].name);
        free(data->meshes[0].primitives[0].attributes[1].data->name);
        free(data->meshes[0].primitives[0].attributes[2].name);
        free(data->meshes[0].primitives[0].attributes[2].data->name);
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
    free(data->meshes[0].primitives[0].attributes[1].name);
    free(data->meshes[0].primitives[0].attributes[1].data->name);
    free(data->meshes[0].primitives[0].attributes[2].name);
    free(data->meshes[0].primitives[0].attributes[2].data->name);
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