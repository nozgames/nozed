//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

constexpr int MAX_VERTICES = 65536;
constexpr int MAX_TRIANGLES = MAX_VERTICES / 3;
constexpr int MAX_INDICES = MAX_TRIANGLES * 3;
constexpr int MAX_EDGES = MAX_VERTICES * 2;

struct View;

struct EditableVertex
{
    Vec2 position;
};

struct EditableEdge
{
    int v0;
    int v1;
};

struct EditableTriangle
{
    int v0;
    int v1;
    int v2;
    Vec2Int color;
};

struct EditableMesh
{
    EditableVertex vertices[MAX_VERTICES];
    EditableEdge edges[MAX_EDGES];
    EditableTriangle triangles[MAX_TRIANGLES];
    MeshBuilder* builder;
    Mesh* mesh;
    Mesh* edge_mesh;
    int vertex_count;
    int edge_count;
    int triangle_count;
    bool dirty;
};


#include "game_assets.h"

extern void InitGrid(Allocator* allocator);
extern void ShutdownGrid();
extern void DrawGrid(Camera* camera, float zoom);
