//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

constexpr int MAX_ASSETS = 1024;
constexpr int MAX_VERTICES = 65536;
constexpr int MAX_TRIANGLES = MAX_VERTICES / 3;
constexpr int MAX_INDICES = MAX_TRIANGLES * 3;
constexpr int MAX_EDGES = MAX_VERTICES * 2;

constexpr int UI_REF_WIDTH = 1920;
constexpr int UI_REF_HEIGHT = 1080;

struct View;

struct EditableVertex
{
    Vec2 position;
    Vec2 saved_position;
    bool selected;
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
    int vertex_count;
    int edge_count;
    int triangle_count;
    bool dirty;
    Bounds2 bounds;
    bool modified;
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
    bool selected;
};

struct AssetEditor
{
    Camera* camera;
    Material* material;
    Material* vertex_material;
    Mesh* vertex_mesh;
    Mesh* edge_mesh;
    float zoom;
    float zoom_ref_scale;
    float ui_scale;
    float dpi;
    InputSet* input;
    int selected_vertex;
    bool dragging;
    Vec2 drag_start;
    Vec2 drag_position_start;
    i32 hover_asset;
    int edit_asset_index;
    Vec2 world_mouse_position;

    EditableAsset* assets[MAX_ASSETS];
    u32 asset_count;
    u32 selected_asset_count;

    // Panning
    bool panning;
    Vec2 pan_start_mouse;
    Vec2 pan_start_camera;

    // Box select
    bool box_selecting;
    Vec2 box_start_mouse;
    Vec2 box_start_world;
    Bounds2 box_selection;
};

#include "editor_assets.h"

// @editor
extern void UpdateBoxSelect(InputSet* set);
extern void ClearBoxSelect();

// @grid
extern void InitGrid(Allocator* allocator);
extern void ShutdownGrid();
extern void DrawGrid(Camera* camera);

extern AssetEditor g_asset_editor;

constexpr Color COLOR_SELECTED = { 1.0f, 0.788f, 0.055f, 1.0f};


// @editable_asset
extern bool HitTestAsset(const EditableAsset& ea, const Vec2& hit_pos);
extern bool HitTestAsset(const EditableAsset& ea, const Bounds2& hit_bounds);
extern int HitTestAssets(const Vec2& hit_pos);
extern int HitTestAssets(const Bounds2& bit_bounds);
extern void DrawEdges(const EditableAsset& ea, int min_edge_count, Color color);
extern void DrawAsset(const EditableAsset& ea);
extern Bounds2 GetBounds(const EditableAsset& ea);
extern int GetFirstSelectedAsset();
extern Bounds2 GetSelectedBounds(const EditableAsset& ea);

// @editable_mesh
extern bool HitTest(const EditableMesh& mesh, const Vec2& position, const Bounds2& hit_bounds);
extern Mesh* ToMesh(EditableMesh* emesh);
extern Bounds2 GetSelectedBounds(const EditableMesh& emesh);
extern void MarkDirty(EditableMesh& emesh);
extern void MarkModified(EditableMesh& emesh);

// @notifications
extern void InitNotifications();
extern void UpdateNotifications();
extern void AddNotification(const char* format, ...);