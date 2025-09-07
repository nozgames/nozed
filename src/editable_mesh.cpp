//
//  MeshZ - Copyright(c) 2025 NoZ Games, LLC
//

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

static void CreateEdges(EditableMesh* emesh)
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
        for (int i = 0; i < emesh->vertex_count; i++)
            AddVertex(emesh->builder, emesh->vertices[i].position, VEC3_UP, VEC2_ZERO, 0);

        for (int i = 0; i <emesh->triangle_count; i++)
        {
            EditableTriangle& tri = emesh->triangles[i];
            AddTriangle(emesh->builder, tri.v0, tri.v1, tri.v2);
        }

        Free(emesh->mesh);
        emesh->mesh = CreateMesh(ALLOCATOR_DEFAULT, emesh->builder, NAME_NONE);

        CreateEdgeMesh(emesh);
    }

    return emesh->mesh;
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

int SplitEdge(EditableMesh* emesh, int edge_index)
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
    new_vertex.position = (v0.position + v1.position) * 0.5f;

    int triangle_count = emesh->triangle_count;
    for (int i = 0; i < triangle_count; i++)
    {
        EditableTriangle& et = emesh->triangles[i];

        int triangle_edge = GetTriangleEdgeIndex(et, ee);
        if (triangle_edge == -1)
            continue;

        EditableTriangle& split_tri = emesh->triangles[emesh->triangle_count++];

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
