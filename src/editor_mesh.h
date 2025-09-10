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
    Mesh* mesh;
    int vertex_count;
    int edge_count;
    int face_count;
    bool dirty;
    Bounds2 bounds;
    bool modified;
    int selected_vertex_count;
};

// @editor_mesh
extern EditorMesh* CreateEditableMesh(Allocator* allocator);
extern bool HitTest(const EditorMesh& mesh, const Vec2& position, const Bounds2& hit_bounds);
extern bool HitTestTriangle(const EditorMesh& em, const EditorFace& et, const Vec2& position, const Vec2& hit_pos, Vec2* where = nullptr);
extern int HitTestTriangle(const EditorMesh& mesh, const Vec2& position, const Vec2& hit_pos, Vec2* where = nullptr);
extern int HitTestEdge(const EditorMesh& em, const Vec2& hit_pos, float size, float* where=nullptr);
extern Mesh* ToMesh(EditorMesh& em, bool upload=true);
extern Bounds2 GetSelectedBounds(const EditorMesh& emesh);
extern void MarkDirty(EditorMesh& emesh);
extern void MarkModified(EditorMesh& emesh);
extern void SetSelectedTrianglesColor(EditorMesh& em, const Vec2Int& color);
extern void MergeSelectedVerticies(EditorMesh& em);
extern void DissolveSelectedVertices(EditorMesh& em);
extern void SetHeight(EditorMesh& em, int index, float height);
extern int SplitEdge(EditorMesh& em, int edge_index, float edge_pos);
extern int SplitTriangle(EditorMesh& em, int triangle_index, const Vec2& position);
extern int AddVertex(EditorMesh& em, const Vec2& position);
extern void SetSelection(EditorMesh& em, int vertex_index);
extern void AddSelection(EditorMesh& em, int vertex_index);
extern void ToggleSelection(EditorMesh& em, int vertex_index);
extern void ClearSelection(EditorMesh& em);
extern void SelectAll(EditorMesh& em);
extern void RotateEdge(EditorMesh& em, int edge_index);
extern EditorMesh* Clone(Allocator* allocator, const EditorMesh& em);
extern void Copy(EditorMesh& dst, const EditorMesh& src);
extern EditorMesh* LoadEditorMesh(Allocator* allocator, const std::filesystem::path& path);
extern void SaveEditorMesh(const EditorMesh& em, const std::filesystem::path& path);
extern EditorAsset* CreateNewEditorMesh(const std::filesystem::path& path);