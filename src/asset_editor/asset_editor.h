//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

constexpr int MAX_ASSETS = 1024;
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
    int triangle_count;
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

enum EditableAssetType
{
    EDITABLE_ASSET_TYPE_UNKNOWN = -1,
    EDITABLE_ASSET_TYPE_MESH,
    EDITABLE_ASSET_TYPE_COUNT,
};


struct EditableAsset
{
    const Name* name;
    EditableAssetType type;
    EditableMesh* mesh;
    Vec2 position;
    bool dirty;
    std::filesystem::path path;
};

struct AssetEditor
{
    Camera* camera;
    Material* material;
    Material* vertex_material;
    Mesh* vertex_mesh;
    Mesh* edge_mesh;
    float zoom;
    InputSet* input;
    int selected_vertex;
    bool dragging;
    Vec2 drag_start;
    Vec2 drag_position_start;
    EditableAsset* assets[MAX_ASSETS];
    u32 asset_count;
    i32 hover_asset;
    i32 selected_asset;
};

#include "game_assets.h"

extern void InitGrid(Allocator* allocator);
extern void ShutdownGrid();
extern void DrawGrid(Camera* camera, float zoom);

extern AssetEditor g_asset_editor;
