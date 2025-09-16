//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

struct EditorAsset;

constexpr int MAX_VERTICES = 4096;
constexpr int MAX_TRIANGLES = MAX_VERTICES / 3;
constexpr int MAX_INDICES = MAX_TRIANGLES * 3;
constexpr int MAX_EDGES = MAX_VERTICES * 2;

struct EditorVertex
{
    Vec2 position;
    Vec2 saved_position;
    float height;
    float saved_height;
    bool selected;
    Vec2 edge_normal;
    float edge_size;
};

struct EditorEdge
{
    int v0;
    int v1;
    int triangle_count;
    Vec2 normal;
};

struct EditorFace
{
    int v0;
    int v1;
    int v2;
    Vec2Int color;
    Vec3 normal;
};

struct EditorMesh
{
    EditorVertex vertices[MAX_VERTICES];
    EditorEdge edges[MAX_EDGES];
    EditorFace faces[MAX_TRIANGLES];
    int vertex_count;
    int edge_count;
    int face_count;
    Bounds2 bounds;
    int selected_vertex_count;
    Mesh* mesh;
};

// @editor_mesh
extern EditorAsset* NewEditorMesh(const std::filesystem::path& path);
extern EditorAsset* CreateEditableMeshAsset(const std::filesystem::path& path, EditorMesh* em);
extern EditorMesh* Clone(Allocator* allocator, const EditorMesh& em);
extern EditorMesh* LoadEditorMesh(Allocator* allocator, const std::filesystem::path& path);
extern Mesh* ToMesh(EditorMesh& em, bool upload=true);
extern int HitTestFace(EditorMesh& em, const Vec2& position, const Vec2& hit_pos, Vec2* where = nullptr);
extern int HitTestVertex(EditorMesh& em, const Vec2& world_pos);
extern int HitTestEdge(EditorMesh& em, const Vec2& hit_pos, float* where=nullptr);
extern Bounds2 GetSelectedBounds(const EditorMesh& em);
extern void MarkDirty(EditorMesh& em);
extern void SetSelectedTrianglesColor(EditorMesh& em, const Vec2Int& color);
extern void MergeSelectedVerticies(EditorMesh& em);
extern void DissolveSelectedVertices(EditorMesh& em);
extern void SetHeight(EditorMesh& em, int index, float height);
extern int SplitEdge(EditorMesh& em, int edge_index, float edge_pos);
extern int SplitTriangle(EditorMesh& em, int triangle_index, const Vec2& position);
extern int AddVertex(EditorMesh& em, const Vec2& position);
extern void SetSelection(EditorMesh& em, int vertex_index);
extern void AddSelection(EditorMesh& em, int vertex_index);
extern void RemoveSelection(EditorMesh& em, int vertex_index);
extern void ToggleSelection(EditorMesh& em, int vertex_index);
extern void ClearSelection(EditorMesh& em);
extern void SelectAll(EditorMesh& em);
extern int RotateEdge(EditorMesh& em, int edge_index);
