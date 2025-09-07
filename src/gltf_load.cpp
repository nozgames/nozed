//
//  MeshZ - Copyright(c) 2025 NoZ Games, LLC
//

#include "pch.h"

extern void CreateEdges(EditableMesh* emesh);

#define CGLTF_IMPLEMENTATION
#include <cgltf.h>

EditableMesh* LoadEditableMesh(Allocator* allocator, const char* filename)
{
    if (!filename || !allocator)
        return nullptr;

    // Parse the glTF file
    cgltf_options options = {};
    cgltf_data* data = nullptr;
    
    cgltf_result result = cgltf_parse_file(&options, filename, &data);
    if (result != cgltf_result_success || !data)
        return nullptr;

    // Load buffers
    result = cgltf_load_buffers(&options, data, filename);
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
    
    // Find position attribute
    cgltf_accessor* position_accessor = nullptr;
    for (cgltf_size i = 0; i < primitive->attributes_count; ++i)
    {
        if (primitive->attributes[i].type == cgltf_attribute_type_position)
        {
            position_accessor = primitive->attributes[i].data;
            break;
        }
    }
    
    if (!position_accessor)
    {
        cgltf_free(data);
        Free(mesh);
        return nullptr;
    }

    // Load vertices
    mesh->vertex_count = (int)position_accessor->count;
    if (mesh->vertex_count > MAX_VERTICES)
        mesh->vertex_count = MAX_VERTICES;
    
    // Unpack position data
    cgltf_size num_floats = cgltf_accessor_unpack_floats(position_accessor, nullptr, 0);
    float* positions = (float*)malloc(num_floats * sizeof(float));
    cgltf_accessor_unpack_floats(position_accessor, positions, num_floats);
    
    // Copy vertices (assuming Vec2 positions)
    for (int i = 0; i < mesh->vertex_count; ++i)
    {
        if (position_accessor->type == cgltf_type_vec2)
        {
            mesh->vertices[i].position.x = positions[i * 2 + 0];
            mesh->vertices[i].position.y = positions[i * 2 + 1];
        }
        else if (position_accessor->type == cgltf_type_vec3)
        {
            mesh->vertices[i].position.x = positions[i * 3 + 0];
            mesh->vertices[i].position.y = positions[i * 3 + 1];
            // Ignore Z component for 2D mesh
        }
    }
    
    free(positions);

    // Load indices if present
    if (primitive->indices)
    {
        // Try using unpack_floats instead and convert to integers
        cgltf_size num_floats = cgltf_accessor_unpack_floats(primitive->indices, nullptr, 0);
        if (num_floats > 0)
        {
            float* float_indices = (float*)malloc(num_floats * sizeof(float));
            cgltf_accessor_unpack_floats(primitive->indices, float_indices, num_floats);
            
            mesh->triangle_count = (int)(num_floats / 3);
            if (mesh->triangle_count > MAX_TRIANGLES)
                mesh->triangle_count = MAX_TRIANGLES;
            
            for (int i = 0; i < mesh->triangle_count; ++i)
            {
                int v0 = (int)float_indices[i * 3 + 0];
                int v1 = (int)float_indices[i * 3 + 1];
                int v2 = (int)float_indices[i * 3 + 2];
                
                // Validate indices are within bounds
                if (v0 >= 0 && v0 < mesh->vertex_count &&
                    v1 >= 0 && v1 < mesh->vertex_count &&
                    v2 >= 0 && v2 < mesh->vertex_count)
                {
                    mesh->triangles[i].v0 = v0;
                    mesh->triangles[i].v1 = v1;
                    mesh->triangles[i].v2 = v2;
                }
                else
                {
                    // Invalid triangle - set to 0,0,0 or skip
                    mesh->triangles[i].v0 = 0;
                    mesh->triangles[i].v1 = 0;
                    mesh->triangles[i].v2 = 0;
                }
            }
            
            free(float_indices);
        }
    }
    else
    {
        // Generate triangles from vertex order if no indices
        mesh->triangle_count = mesh->vertex_count / 3;
        if (mesh->triangle_count > MAX_TRIANGLES)
            mesh->triangle_count = MAX_TRIANGLES;
        
        for (int i = 0; i < mesh->triangle_count; ++i)
        {
            mesh->triangles[i].v0 = i * 3 + 0;
            mesh->triangles[i].v1 = i * 3 + 1;
            mesh->triangles[i].v2 = i * 3 + 2;
        }
    }

    mesh->dirty = true;
    CreateEdges(mesh);

    cgltf_free(data);
    return mesh;
}