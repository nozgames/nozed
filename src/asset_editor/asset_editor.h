//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

constexpr int STATE_STACK_SIZE = 16;
constexpr int MAX_ASSETS = 1024;
constexpr int MAX_VERTICES = 4096;
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
    float height;
    float saved_height;
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
    Vec3 normal;
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
    int selected_vertex_count;
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
    Vec2 saved_position;
    bool dirty;
    std::filesystem::path path;
    bool selected;
};

enum AssetEditorState
{
    ASSET_EDITOR_STATE_DEFAULT,
    ASSET_EDITOR_STATE_MOVE,
    ASSET_EDITOR_STATE_EDIT,
    ASSET_EDITOR_STATE_BOX_SELECT,
    ASSET_EDITOR_STATE_PAN,
};

struct AssetEditor
{
    AssetEditorState state_stack[STATE_STACK_SIZE];
    int state_stack_count;
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
    int edit_asset_index;
    bool clear_selection_on_release;
    Vec2 pan_start;

    EditableAsset* assets[MAX_ASSETS];
    u32 asset_count;
    u32 selected_asset_count;

    Bounds2 box_selection;
    Vec2 move_world_position;

    bool drag;
    Vec2 drag_position;
    Vec2 drag_world_position;
    Vec2 drag_delta;
    Vec2 drag_world_delta;
    Vec2 mouse_position;
    Vec2 mouse_world_position;
};

#include "editor_assets.h"


extern AssetEditor g_asset_editor;

constexpr Color COLOR_SELECTED = { 1,1,1,1 };

// @editor
extern void InitAssetEditor();
extern void UpdateAssetEditor();
extern void ShutdownAssetEditor();
extern void ClearBoxSelect();
extern void PushState(AssetEditorState state);
extern void PopState();

// @grid
extern void InitGrid(Allocator* allocator);
extern void ShutdownGrid();
extern void DrawGrid(Camera* camera);

// @editable_asset
extern i32 LoadEditableAssets(EditableAsset** assets);
extern void SaveEditableAssets();
extern bool HitTestAsset(const EditableAsset& ea, const Vec2& hit_pos);
extern bool HitTestAsset(const EditableAsset& ea, const Bounds2& hit_bounds);
extern int HitTestAssets(const Vec2& hit_pos);
extern int HitTestAssets(const Bounds2& bit_bounds);
extern void DrawEdges(const EditableAsset& ea, int min_edge_count, Color color);
extern void DrawAsset(const EditableAsset& ea);
extern Bounds2 GetBounds(const EditableAsset& ea);
extern int GetFirstSelectedAsset();
extern Bounds2 GetSelectedBounds(const EditableAsset& ea);
extern void MoveTo(EditableAsset& asset, const Vec2& position);
extern void ClearAssetSelection();
extern void SetAssetSelection(int asset_index);
extern void AddAssetSelection(int asset_index);

// @editable_mesh
extern EditableMesh* CreateEditableMesh(Allocator* allocator);
extern bool HitTest(const EditableMesh& mesh, const Vec2& position, const Bounds2& hit_bounds);
extern bool HitTestTriangle(const EditableMesh& em, const EditableTriangle& et, const Vec2& position, const Vec2& hit_pos, Vec2* where = nullptr);
extern int HitTestTriangle(const EditableMesh& mesh, const Vec2& position, const Vec2& hit_pos, Vec2* where = nullptr);
extern int HitTestEdge(const EditableMesh& em, const Vec2& hit_pos, float size, float* where=nullptr);
extern Mesh* ToMesh(EditableMesh* emesh);
extern Bounds2 GetSelectedBounds(const EditableMesh& emesh);
extern void MarkDirty(EditableMesh& emesh);
extern void MarkModified(EditableMesh& emesh);
extern void SetSelectedTrianglesColor(EditableMesh& em, const Vec2Int& color);
extern void MergeSelectedVerticies(EditableMesh& em);
extern void DissolveSelectedVertices(EditableMesh& em);
extern void SetHeight(EditableMesh& em, int index, float height);
extern int SplitEdge(EditableMesh& em, int edge_index, float edge_pos);
extern int SplitTriangle(EditableMesh& em, int triangle_index, const Vec2& position);
extern int AddVertex(EditableMesh& em, const Vec2& position);
extern void SetSelection(EditableMesh& em, int vertex_index);
extern void AddSelection(EditableMesh& em, int vertex_index);
extern void ToggleSelection(EditableMesh& em, int vertex_index);
extern void ClearSelection(EditableMesh& em);
extern void SelectAll(EditableMesh& em);
extern void RotateEdge(EditableMesh& em, int edge_index);

// @notifications
extern void InitNotifications();
extern void UpdateNotifications();
extern void AddNotification(const char* format, ...);

// @mesh_editor
extern void UpdateMeshEditor(EditableAsset& ea);
extern void InitMeshEditor(EditableAsset& ea);
extern void DrawMeshEditor(EditableAsset& ea);
extern void HandleMeshEditorBoxSelect(EditableAsset& ea, const Bounds2& bounds);

// @draw
extern void DrawLine(const Vec2& v0, const Vec2& v1, f32 width);
extern void DrawVertex(const Vec2& v, f32 size);
