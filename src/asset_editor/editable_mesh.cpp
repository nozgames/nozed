//
//  MeshZ - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"

static void CreateEdgeMesh(EditableMesh* emesh)
{
    Clear(emesh->builder);

    for (int i=0; i<emesh->edge_count; i++)
    {
        EditableEdge& ee = emesh->edges[i];
        Vec2 p0 = emesh->vertices[ee.v0].position;
        Vec2 p1 = emesh->vertices[ee.v1].position;

        // add a quad for the edge
        Vec2 dir = Normalize(p1 - p0);
        Vec2 normal = Vec2{-dir.y, dir.x};
        float half_thickness = 0.01f;
        Vec2 v0 = p0 + normal * half_thickness;
        Vec2 v1 = p1 + normal * half_thickness;
        Vec2 v2 = p1 - normal * half_thickness;
        Vec2 v3 = p0 - normal * half_thickness;

        AddVertex(emesh->builder, v0, VEC3_FORWARD, VEC2_ZERO, 0);
        AddVertex(emesh->builder, v1, VEC3_FORWARD, VEC2_ZERO, 0);
        AddVertex(emesh->builder, v2, VEC3_FORWARD, VEC2_ZERO, 0);
        AddVertex(emesh->builder, v3, VEC3_FORWARD, VEC2_ZERO, 0);

        AddTriangle(emesh->builder, i*4+0, i*4+1, i*4+2);
        AddTriangle(emesh->builder, i*4+0, i*4+2, i*4+3);
    }

    emesh->edge_mesh = CreateMesh(ALLOCATOR_DEFAULT, emesh->builder, NAME_NONE);
}

static int GetOrAddIndex(EditableMesh* emesh, int v0, int v1)
{
    int fv0 = Min(v0, v1);
    int fv1 = Max(v0, v1);

    for (int i = 0; i < emesh->edge_count; i++)
    {
        EditableEdge& ee = emesh->edges[i];
        if (ee.v0 == fv0 && ee.v1 == fv1)
            return i;
    }

    // Not found - add it
    if (emesh->edge_count >= MAX_EDGES)
        return -1;

    int edge_index = emesh->edge_count++;
    EditableEdge& ee = emesh->edges[edge_index];
    ee.v0 = fv0;
    ee.v1 = fv1;
    return edge_index;
}

void CreateEdges(EditableMesh* emesh)
{
    emesh->edge_count = 0;
    for (int i = 0; i < emesh->triangle_count; i++)
    {
        EditableTriangle& et = emesh->triangles[i];

        int e0 = GetOrAddIndex(emesh, et.v0, et.v1);
        int e1 = GetOrAddIndex(emesh, et.v1, et.v2);
        int e2 = GetOrAddIndex(emesh, et.v2, et.v0);
    }
}

Mesh* ToMesh(EditableMesh* emesh)
{
    if (emesh->dirty)
    {
        Clear(emesh->builder);

        for (int i = 0; i <emesh->triangle_count; i++)
        {
            EditableTriangle& tri = emesh->triangles[i];

            Vec2 uv_color = ColorUV(tri.color.x, tri.color.y);
            AddVertex(emesh->builder, emesh->vertices[tri.v0].position, VEC3_UP, uv_color, 0);
            AddVertex(emesh->builder, emesh->vertices[tri.v1].position, VEC3_UP, uv_color, 0);
            AddVertex(emesh->builder, emesh->vertices[tri.v2].position, VEC3_UP, uv_color, 0);
            AddTriangle(emesh->builder, i * 3, i * 3 + 1, i * 3 + 2);
        }

        Free(emesh->mesh);
        emesh->mesh = CreateMesh(ALLOCATOR_DEFAULT, emesh->builder, NAME_NONE);
    }

    return emesh->mesh;
}

void SetTriangleColor(EditableMesh* emesh, int index, const Vec2Int& color)
{
    if (index < 0 || index >= emesh->triangle_count)
        return;

    emesh->triangles[index].color = color;
    emesh->dirty = true;
}

void SetPosition(EditableMesh* emesh, int index, const Vec2& position)
{
    if (index < 0 || index >= emesh->vertex_count)
        return;

    emesh->vertices[index].position = position;
    emesh->dirty = true;
}

void DissolveVertex(EditableMesh* mesh, int vertex_index)
{
    assert(mesh);
    assert(vertex_index >= 0 && vertex_index < mesh->vertex_count);

    /*

    In Blender, Dissolve Vertex is a mesh editing operation that removes selected vertices while trying to preserve the overall shape of the mesh as much as possible. Here's how it works:
    What it does
    When you dissolve a vertex, Blender removes that vertex and reconnects the surrounding geometry in the simplest way possible. Unlike deleting a vertex (which would create a hole), dissolving attempts to maintain mesh continuity by merging the faces that were connected to that vertex.
    */

    // Build ordered list of boundary edges around the vertex
    int boundary_edges[64][2]; // [edge_index][v0,v1]
    int boundary_count = 0;
    
    // Find all triangles using this vertex and collect their edges
    for (int i = 0; i < mesh->triangle_count; i++)
    {
        EditableTriangle& et = mesh->triangles[i];
        if (et.v0 == vertex_index || et.v1 == vertex_index || et.v2 == vertex_index)
        {
            // Get the edge opposite to the vertex being dissolved
            // This edge will become part of the boundary of the hole
            if (et.v0 == vertex_index) {
                // Opposite edge is v1->v2
                if (boundary_count < 64) {
                    boundary_edges[boundary_count][0] = et.v1;
                    boundary_edges[boundary_count][1] = et.v2;
                    boundary_count++;
                }
            } else if (et.v1 == vertex_index) {
                // Opposite edge is v2->v0  
                if (boundary_count < 64) {
                    boundary_edges[boundary_count][0] = et.v2;
                    boundary_edges[boundary_count][1] = et.v0;
                    boundary_count++;
                }
            } else { // et.v2 == vertex_index
                // Opposite edge is v0->v1
                if (boundary_count < 64) {
                    boundary_edges[boundary_count][0] = et.v0;
                    boundary_edges[boundary_count][1] = et.v1;
                    boundary_count++;
                }
            }
        }
    }
    
    // Remove all triangles that reference this vertex
    for (int i = mesh->triangle_count - 1; i >= 0; i--)
    {
        EditableTriangle& et = mesh->triangles[i];
        if (et.v0 == vertex_index || et.v1 == vertex_index || et.v2 == vertex_index)
        {
            // Remove triangle by swapping with last and reducing count
            mesh->triangles[i] = mesh->triangles[mesh->triangle_count - 1];
            mesh->triangle_count--;
        }
    }
    
    // Remove duplicate edges (edges that appear twice are internal, not boundary)
    int filtered_edges[64][2];
    int filtered_count = 0;
    
    for (int i = 0; i < boundary_count; i++)
    {
        bool is_duplicate = false;
        for (int j = i + 1; j < boundary_count; j++)
        {
            // Check if edge i and j are the same (in either direction)
            if ((boundary_edges[i][0] == boundary_edges[j][0] && boundary_edges[i][1] == boundary_edges[j][1]) ||
                (boundary_edges[i][0] == boundary_edges[j][1] && boundary_edges[i][1] == boundary_edges[j][0]))
            {
                is_duplicate = true;
                break;
            }
        }
        
        if (!is_duplicate)
        {
            filtered_edges[filtered_count][0] = boundary_edges[i][0];
            filtered_edges[filtered_count][1] = boundary_edges[i][1];
            filtered_count++;
        }
    }
    
    // Order the boundary edges to form a continuous loop
    int ordered_vertices[64];
    int ordered_count = 0;
    
    if (filtered_count >= 2)
    {
        // Start with first edge
        ordered_vertices[ordered_count++] = filtered_edges[0][0];
        ordered_vertices[ordered_count++] = filtered_edges[0][1];
        bool used[64] = {true}; // Mark first edge as used
        for (int i = 1; i < filtered_count; i++) used[i] = false;
        
        // Find connecting edges
        while (ordered_count < filtered_count + 1)
        {
            int last_vertex = ordered_vertices[ordered_count - 1];
            bool found = false;
            
            for (int i = 1; i < filtered_count; i++)
            {
                if (used[i]) continue;
                
                if (filtered_edges[i][0] == last_vertex)
                {
                    ordered_vertices[ordered_count++] = filtered_edges[i][1];
                    used[i] = true;
                    found = true;
                    break;
                } else if (filtered_edges[i][1] == last_vertex)
                {
                    ordered_vertices[ordered_count++] = filtered_edges[i][0];  
                    used[i] = true;
                    found = true;
                    break;
                }
            }
            
            if (!found) break;
        }
        
        // Remove duplicate last vertex if it connects back to first
        if (ordered_count > 2 && ordered_vertices[ordered_count - 1] == ordered_vertices[0])
            ordered_count--;
        
        // Triangulate the ordered polygon using fan triangulation
        for (int i = 1; i < ordered_count - 1; i++)
        {
            if (mesh->triangle_count < MAX_TRIANGLES) {
                EditableTriangle& new_tri = mesh->triangles[mesh->triangle_count++];
                new_tri.v0 = ordered_vertices[0];
                new_tri.v1 = ordered_vertices[i];
                new_tri.v2 = ordered_vertices[i + 1];
            }
        }
    }
    
    // Shift all vertices down as long as it wasn't the last vertex
    for (int i = vertex_index; i < mesh->vertex_count - 1; i++)
        mesh->vertices[i] = mesh->vertices[i + 1];
    
    mesh->vertex_count--;
    
    // Update all triangle vertex indices that are greater than vertex_index
    for (int i = 0; i < mesh->triangle_count; i++)
    {
        EditableTriangle& et = mesh->triangles[i];
        if (et.v0 > vertex_index) et.v0--;
        if (et.v1 > vertex_index) et.v1--;
        if (et.v2 > vertex_index) et.v2--;
    }
    
    mesh->dirty = true;
    CreateEdges(mesh);
}

void DeleteVertex(EditableMesh* mesh, int vertex_index)
{
    assert(mesh);
    assert(vertex_index >= 0 && vertex_index < mesh->vertex_count);

    // Remove any triangles that reference this vertex
    for (int i = mesh->triangle_count - 1; i >= 0; i--)
    {
        EditableTriangle& et = mesh->triangles[i];
        if (et.v0 == vertex_index || et.v1 == vertex_index || et.v2 == vertex_index)
        {
            // Remove triangle by swapping with last and reducing count
            mesh->triangles[i] = mesh->triangles[mesh->triangle_count - 1];
            mesh->triangle_count--;
        }
    }

    // Iterate over all triangles and decrement vertex indices greater than vertex_index
    for (int i = 0; i < mesh->triangle_count; i++)
    {
        EditableTriangle& et = mesh->triangles[i];
        if (et.v0 > vertex_index) et.v0--;
        if (et.v1 > vertex_index) et.v1--;
        if (et.v2 > vertex_index) et.v2--;
    }

    // shift all verticies down as long as it wasnt the last vertex
    for (int i = vertex_index; i < mesh->vertex_count - 1; i++)
        mesh->vertices[i] = mesh->vertices[i + 1];

    mesh->vertex_count--;

    // We could end up with vertices that no longer have a triangle, we need to find them and remove them
    for (int i = mesh->vertex_count - 1; i >= 0; i--)
    {
        bool used = false;
        for (int j = 0; j < mesh->triangle_count; j++)
        {
            EditableTriangle& et = mesh->triangles[j];
            if (et.v0 == i || et.v1 == i || et.v2 == i)
            {
                used = true;
                break;
            }
        }

        if (!used)
        {
            // Remove vertex by swapping with last and reducing count
            mesh->vertices[i] = mesh->vertices[mesh->vertex_count - 1];
            mesh->vertex_count--;

            // Iterate over all triangles and decrement vertex indices greater than i
            for (int j = 0; j < mesh->triangle_count; j++)
            {
                EditableTriangle& et = mesh->triangles[j];
                if (et.v0 > i) et.v0--;
                if (et.v1 > i) et.v1--;
                if (et.v2 > i) et.v2--;
            }
        }
    }

    mesh->dirty = true;
    CreateEdges(mesh);
}

static int GetTriangleEdgeIndex(const EditableTriangle& et, const EditableEdge& ee)
{
    // Return 0, 1, or 2 if the edge matches one of the triangle's edges
    if ((et.v0 == ee.v0 && et.v1 == ee.v1) || (et.v0 == ee.v1 && et.v1 == ee.v0))
        return 0;

    if ((et.v1 == ee.v0 && et.v2 == ee.v1) || (et.v1 == ee.v1 && et.v2 == ee.v0))
        return 1;

    if ((et.v2 == ee.v0 && et.v0 == ee.v1) || (et.v2 == ee.v1 && et.v0 == ee.v0))
        return 2;

    return -1;
}


void RotateEdge(EditableMesh* mesh, int edge_index)
{
    assert(mesh);
    assert(edge_index >= 0 && edge_index < mesh->edge_count);
    
    EditableEdge& edge = mesh->edges[edge_index];
    
    // Find the two triangles that share this edge
    int triangle_indices[2];
    int triangle_count = 0;
    
    for (int i = 0; i < mesh->triangle_count; i++)
    {
        EditableTriangle& et = mesh->triangles[i];
        if (GetTriangleEdgeIndex(et, edge) != -1)
        {
            if (triangle_count < 2)
            {
                triangle_indices[triangle_count] = i;
                triangle_count++;
            }
        }
    }
    
    // Edge rotation only works if exactly 2 triangles share the edge
    if (triangle_count != 2)
        return;
    
    EditableTriangle& tri1 = mesh->triangles[triangle_indices[0]];
    EditableTriangle& tri2 = mesh->triangles[triangle_indices[1]];
    
    // Find the vertices that are NOT part of the shared edge
    int opposite1 = -1, opposite2 = -1;
    
    // Find opposite vertex in first triangle
    if (tri1.v0 != edge.v0 && tri1.v0 != edge.v1)
        opposite1 = tri1.v0;
    else if (tri1.v1 != edge.v0 && tri1.v1 != edge.v1)
        opposite1 = tri1.v1;
    else if (tri1.v2 != edge.v0 && tri1.v2 != edge.v1)
        opposite1 = tri1.v2;
    
    // Find opposite vertex in second triangle
    if (tri2.v0 != edge.v0 && tri2.v0 != edge.v1)
        opposite2 = tri2.v0;
    else if (tri2.v1 != edge.v0 && tri2.v1 != edge.v1)
        opposite2 = tri2.v1;
    else if (tri2.v2 != edge.v0 && tri2.v2 != edge.v1)
        opposite2 = tri2.v2;
    
    if (opposite1 == -1 || opposite2 == -1)
        return;
    
    // Create new triangles with the rotated edge, maintaining proper winding order
    // The new edge connects opposite1 and opposite2
    
    // We need to determine the correct winding order by checking the original triangles
    // Get positions to calculate winding
    Vec2 pos_opposite1 = mesh->vertices[opposite1].position;
    Vec2 pos_opposite2 = mesh->vertices[opposite2].position;
    Vec2 pos_v0 = mesh->vertices[edge.v0].position;
    Vec2 pos_v1 = mesh->vertices[edge.v1].position;
    
    // Calculate cross product to determine winding for first triangle
    // We want: opposite1 -> edge.v0 -> opposite2 to maintain CCW winding
    Vec2 edge1 = {pos_v0.x - pos_opposite1.x, pos_v0.y - pos_opposite1.y};
    Vec2 edge2 = {pos_opposite2.x - pos_opposite1.x, pos_opposite2.y - pos_opposite1.y};
    float cross1 = edge1.x * edge2.y - edge1.y * edge2.x;
    
    if (cross1 > 0) {
        // CCW winding
        tri1.v0 = opposite1;
        tri1.v1 = edge.v0;
        tri1.v2 = opposite2;
    } else {
        // CW winding - reverse order
        tri1.v0 = opposite1;
        tri1.v1 = opposite2;
        tri1.v2 = edge.v0;
    }
    
    // Calculate winding for second triangle  
    // We want: opposite1 -> opposite2 -> edge.v1 or opposite1 -> edge.v1 -> opposite2
    Vec2 edge3 = {pos_opposite2.x - pos_opposite1.x, pos_opposite2.y - pos_opposite1.y};
    Vec2 edge4 = {pos_v1.x - pos_opposite1.x, pos_v1.y - pos_opposite1.y};
    float cross2 = edge3.x * edge4.y - edge3.y * edge4.x;
    
    if (cross2 > 0) {
        // CCW winding
        tri2.v0 = opposite1;
        tri2.v1 = opposite2;
        tri2.v2 = edge.v1;
    } else {
        // CW winding - reverse order
        tri2.v0 = opposite1;
        tri2.v1 = edge.v1;
        tri2.v2 = opposite2;
    }
    
    mesh->dirty = true;
    CreateEdges(mesh);
}

int SplitEdge(EditableMesh* emesh, int edge_index, float edge_pos)
{
    assert(emesh);
    assert(edge_index >=0 && edge_index < emesh->edge_count);

    if (emesh->vertex_count >= MAX_VERTICES)
        return -1;

    if (emesh->triangle_count + 2 >= MAX_TRIANGLES)
        return -1;

    EditableEdge& ee = emesh->edges[edge_index];
    EditableVertex& v0 = emesh->vertices[ee.v0];
    EditableVertex& v1 = emesh->vertices[ee.v1];

    int new_vertex_index = emesh->vertex_count++;
    EditableVertex& new_vertex = emesh->vertices[new_vertex_index];
    new_vertex.position = (v0.position * (1.0f - edge_pos) + v1.position * edge_pos);

    int triangle_count = emesh->triangle_count;
    for (int i = 0; i < triangle_count; i++)
    {
        EditableTriangle& et = emesh->triangles[i];

        int triangle_edge = GetTriangleEdgeIndex(et, ee);
        if (triangle_edge == -1)
            continue;

        EditableTriangle& split_tri = emesh->triangles[emesh->triangle_count++];
        split_tri.color = et.color;

        if (triangle_edge == 0)
        {
            split_tri.v0 = new_vertex_index;
            split_tri.v1 = et.v1;
            split_tri.v2 = et.v2;
            et.v1 = new_vertex_index;
        }
        else if (triangle_edge == 1)
        {
            split_tri.v0 = et.v0;
            split_tri.v1 = new_vertex_index;
            split_tri.v2 = et.v2;
            et.v2 = new_vertex_index;
        }
        else
        {
            split_tri.v0 = et.v0;
            split_tri.v1 = et.v1;
            split_tri.v2 = new_vertex_index;
            et.v0 = new_vertex_index;
        }
    }

    emesh->dirty = true;

    CreateEdges(emesh);

    return new_vertex_index;
}

EditableMesh* CreateEditableMesh(Allocator* allocator)
{
    EditableMesh* emesh = (EditableMesh*)Alloc(allocator, sizeof(EditableMesh));
    emesh->builder = CreateMeshBuilder(ALLOCATOR_DEFAULT, MAX_VERTICES, MAX_INDICES);
    emesh->vertex_count = 4;
    emesh->vertices[0] = {
        .position =  {-0.5f, -0.5f}
    };
    emesh->vertices[1] = {
        .position =  { 0.5f, -0.5f}
    };
    emesh->vertices[2] = {
        .position =  { 0.5f,  0.5f}
    };
    emesh->vertices[3] = {
        .position =  {-0.5f,  0.5f}
    };

    emesh->triangle_count = 2;
    emesh->triangles[0] = { 0, 1, 2};
    emesh->triangles[1] = { 0, 2, 3};
    emesh->dirty = true;

    CreateEdges(emesh);

    return emesh;
}
