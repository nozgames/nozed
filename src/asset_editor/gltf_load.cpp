
//  MeshZ - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"

#include <cgltf.h>

EditableMesh* LoadEditableMesh(Allocator* allocator, const std::filesystem::path& filename)
{
    // Parse the glTF file
    cgltf_options options = {};
    cgltf_data* data = nullptr;
    
    cgltf_result result = cgltf_parse_file(&options, filename.string().c_str(), &data);
    if (result != cgltf_result_success || !data)
        return nullptr;

    // Load buffers
    result = cgltf_load_buffers(&options, data, filename.string().c_str());
    if (result != cgltf_result_success)
    {
        cgltf_free(data);
        return nullptr;
    }

    // Create EditableMesh
    EditableMesh* mesh = (EditableMesh*)Alloc(allocator, sizeof(EditableMesh));
    if (!mesh)
    {
        cgltf_free(data);
        return nullptr;
    }
    
    // Initialize mesh
    memset(mesh, 0, sizeof(EditableMesh));
    mesh->builder = CreateMeshBuilder(ALLOCATOR_DEFAULT, MAX_VERTICES, MAX_INDICES);

    // Find first mesh and primitive
    if (data->meshes_count == 0 || data->meshes[0].primitives_count == 0)
    {
        cgltf_free(data);
        Free(mesh);
        return nullptr;
    }

    cgltf_primitive* primitive = &data->meshes[0].primitives[0];
    
    // Find position and UV attributes (ignore normals - we calculate them on save/ToMesh)
    cgltf_accessor* position_accessor = nullptr;
    cgltf_accessor* uv_accessor = nullptr;
    for (cgltf_size i = 0; i < primitive->attributes_count; ++i)
    {
        if (primitive->attributes[i].type == cgltf_attribute_type_position)
        {
            position_accessor = primitive->attributes[i].data;
        }
        else if (primitive->attributes[i].type == cgltf_attribute_type_texcoord)
        {
            uv_accessor = primitive->attributes[i].data;
        }
        // Ignore normals - we calculate them when needed
    }
    
    if (!position_accessor)
    {
        cgltf_free(data);
        Free(mesh);
        return nullptr;
    }

    // Load all vertices first (these are separate vertices from save format)
    int loaded_vertex_count = (int)position_accessor->count;
    if (loaded_vertex_count > MAX_VERTICES)
        loaded_vertex_count = MAX_VERTICES;
    
    // Unpack position data
    cgltf_size num_floats = cgltf_accessor_unpack_floats(position_accessor, nullptr, 0);
    float* positions = (float*)malloc(num_floats * sizeof(float));
    cgltf_accessor_unpack_floats(position_accessor, positions, num_floats);
    
    // Load all positions into temporary array (allocate on heap to avoid stack overflow)
    Vec2* loaded_positions = (Vec2*)malloc(MAX_VERTICES * sizeof(Vec2));
    for (int i = 0; i < loaded_vertex_count; ++i)
    {
        if (position_accessor->type == cgltf_type_vec2)
        {
            loaded_positions[i].x = positions[i * 2 + 0];
            loaded_positions[i].y = positions[i * 2 + 1];
        }
        else if (position_accessor->type == cgltf_type_vec3)
        {
            loaded_positions[i].x = positions[i * 3 + 0];
            loaded_positions[i].y = positions[i * 3 + 1];
            // Ignore Z component for 2D mesh
        }
    }
    
    free(positions);
    
    // Load indices first to get the correct triangle mapping
    float* float_indices = nullptr;
    int loaded_triangle_count = 0;
    
    if (primitive->indices)
    {
        cgltf_size num_floats = cgltf_accessor_unpack_floats(primitive->indices, nullptr, 0);
        if (num_floats > 0)
        {
            float_indices = (float*)malloc(num_floats * sizeof(float));
            cgltf_accessor_unpack_floats(primitive->indices, float_indices, num_floats);
            loaded_triangle_count = (int)(num_floats / 3);
        }
    }
    else
    {
        loaded_triangle_count = loaded_vertex_count / 3;
    }
    
    // Load UV coordinates BEFORE vertex merging to preserve triangle-color mapping (allocate on heap)
    Vec2Int* triangle_colors = (Vec2Int*)malloc(MAX_TRIANGLES * sizeof(Vec2Int));
    if (uv_accessor && loaded_triangle_count > 0)
    {
        cgltf_size num_uv_floats = cgltf_accessor_unpack_floats(uv_accessor, nullptr, 0);
        if (num_uv_floats > 0)
        {
            float* uv_data = (float*)malloc(num_uv_floats * sizeof(float));
            cgltf_accessor_unpack_floats(uv_accessor, uv_data, num_uv_floats);
            
            // Extract one color per triangle from UV data using proper indices
            for (int i = 0; i < loaded_triangle_count && i < MAX_TRIANGLES; ++i)
            {
                int vertex_index;
                if (float_indices)
                    vertex_index = (int)float_indices[i * 3 + 0];
                else
                    vertex_index = i * 3;

                if (vertex_index * 2 + 1 < (int)num_uv_floats)
                {
                    float u = uv_data[vertex_index * 2 + 0]; // U of first vertex of triangle i
                    float v = uv_data[vertex_index * 2 + 1]; // V of first vertex of triangle i
                    
                    int col = (int)(u * 16.0f);
                    int row = (int)(v * 16.0f);
                    triangle_colors[i].x = col;
                    triangle_colors[i].y = row;
                }
                else
                {
                    triangle_colors[i] = {0, 0}; // Default color
                }
            }
            
            free(uv_data);
        }
    }
    else
    {
        // Initialize with default colors if no UV data
        for (int i = 0; i < loaded_triangle_count && i < MAX_TRIANGLES; ++i)
        {
            triangle_colors[i] = {0, 0};
        }
    }
    
    // Merge duplicate vertices - build mapping from loaded_index to merged_index (allocate on heap)
    int* vertex_remap = (int*)malloc(MAX_VERTICES * sizeof(int)); // Maps loaded vertex index to merged vertex index
    mesh->vertex_count = 0;
    const float EPSILON = 1e-6f;
    
    for (int i = 0; i < loaded_vertex_count; ++i)
    {
        // Check if this position already exists in merged vertices
        int existing_index = -1;
        for (int j = 0; j < mesh->vertex_count; ++j)
        {
            float dx = loaded_positions[i].x - mesh->vertices[j].position.x;
            float dy = loaded_positions[i].y - mesh->vertices[j].position.y;
            if (dx * dx + dy * dy < EPSILON * EPSILON)
            {
                existing_index = j;
                break;
            }
        }
        
        if (existing_index >= 0)
        {
            // Use existing vertex
            vertex_remap[i] = existing_index;
        }
        else
        {
            // Create new merged vertex
            if (mesh->vertex_count < MAX_VERTICES)
            {
                mesh->vertices[mesh->vertex_count].position = loaded_positions[i];
                vertex_remap[i] = mesh->vertex_count;
                mesh->vertex_count++;
            }
            else
            {
                vertex_remap[i] = 0; // Fallback to vertex 0 if too many vertices
            }
        }
    }

    // Load indices if present (reuse float_indices from above)
    if (primitive->indices && float_indices)
    {
        mesh->triangle_count = loaded_triangle_count;
        if (mesh->triangle_count > MAX_TRIANGLES)
            mesh->triangle_count = MAX_TRIANGLES;
            
        for (int i = 0; i < mesh->triangle_count; ++i)
        {
            int loaded_v0 = (int)float_indices[i * 3 + 0];
            int loaded_v1 = (int)float_indices[i * 3 + 1];
            int loaded_v2 = (int)float_indices[i * 3 + 2];

            // Validate loaded indices are within bounds and remap to merged vertices
            if (loaded_v0 >= 0 && loaded_v0 < loaded_vertex_count &&
                loaded_v1 >= 0 && loaded_v1 < loaded_vertex_count &&
                loaded_v2 >= 0 && loaded_v2 < loaded_vertex_count)
            {
                mesh->triangles[i].v0 = vertex_remap[loaded_v0];
                mesh->triangles[i].v1 = vertex_remap[loaded_v1];
                mesh->triangles[i].v2 = vertex_remap[loaded_v2];

                // Assign color from pre-loaded triangle colors
                if (i < loaded_triangle_count)
                {
                    mesh->triangles[i].color = triangle_colors[i];
                }
            }
            else
            {
                // Invalid triangle - set to 0,0,0 or skip
                mesh->triangles[i].v0 = 0;
                mesh->triangles[i].v1 = 0;
                mesh->triangles[i].v2 = 0;
                mesh->triangles[i].color = {0, 0};
            }
        }
    }
    else
    {
        // Generate triangles from vertex order if no indices
        mesh->triangle_count = loaded_triangle_count;
        if (mesh->triangle_count > MAX_TRIANGLES)
            mesh->triangle_count = MAX_TRIANGLES;
        
        for (int i = 0; i < mesh->triangle_count; ++i)
        {
            // Remap loaded vertex indices to merged vertex indices
            mesh->triangles[i].v0 = vertex_remap[i * 3 + 0];
            mesh->triangles[i].v1 = vertex_remap[i * 3 + 1];
            mesh->triangles[i].v2 = vertex_remap[i * 3 + 2];
            
            // Assign color from pre-loaded triangle colors
            if (i < loaded_triangle_count)
            {
                mesh->triangles[i].color = triangle_colors[i];
            }
        }
    }

    // Clean up indices
    if (float_indices)
        free(float_indices);

    // UV coordinates were already loaded before vertex merging

    MarkDirty(*mesh);

    // Cleanup heap allocated arrays
    free(loaded_positions);
    free(triangle_colors);
    free(vertex_remap);

    cgltf_free(data);
    return mesh;
}
