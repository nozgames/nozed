//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

struct EditorAsset;

constexpr int MAX_VERTICES = 1024;
constexpr int MAX_FACES = MAX_VERTICES / 3;
constexpr int MAX_INDICES = MAX_FACES * 3;
constexpr int MAX_EDGES = MAX_VERTICES * 2;

struct EditorVertex {
    Vec2 position;
    float height;
    Vec2 edge_normal;
    float edge_size;
    bool selected;
    int ref_count;
    float gradient;
};

struct EditorEdge {
    int v0;
    int v1;
    int face_count;
    int face_index[2];
    Vec2 normal;
    bool selected;
};

struct EditorFace {
    Vec2Int color;
    Vec2Int gradient_color;
    Vec2 gradient_dir;
    float gradient_offset;
    Vec3 normal;
    bool selected;
    int vertex_offset;
    int vertex_count;
};

struct EditorMesh : EditorAsset
{
    EditorVertex vertices[MAX_VERTICES];
    EditorEdge edges[MAX_EDGES];
    EditorFace faces[MAX_FACES];
    int face_vertices[MAX_INDICES];
    int face_vertex_count;
    int vertex_count;
    int edge_count;
    int face_count;
    int selected_count;
    Mesh* mesh;
    Vec2Int edge_color;
};

// @editor_mesh
inline EditorMesh* GetEditorMesh(int index)
{
    EditorAsset* ea = GetEditorAsset(index);
    if (!ea)
        return nullptr;

    assert(ea->type == EDITOR_ASSET_TYPE_MESH);
    return (EditorMesh*)ea;
}

extern void InitEditorMesh(EditorAsset* ea);
extern EditorAsset* NewEditorMesh(const std::filesystem::path& path);
extern EditorMesh* Clone(Allocator* allocator, EditorMesh* em);
extern EditorMesh* LoadEditorMesh(const std::filesystem::path& path);
extern Mesh* ToMesh(EditorMesh* em, bool upload=true);
extern int HitTestFace(EditorMesh* em, const Vec2& position, const Vec2& hit_pos, Vec2* where = nullptr);
extern int HitTestVertex(EditorMesh* em, const Vec2& world_pos, float size_mult=1.0f);
extern int HitTestEdge(EditorMesh* em, const Vec2& hit_pos, float* where=nullptr);
extern Bounds2 GetSelectedBounds(EditorMesh* em);
extern void MarkDirty(EditorMesh* em);
extern void SetSelectedTrianglesColor(EditorMesh* em, const Vec2Int& color);
extern void SetEdgeColor(EditorMesh* em, const Vec2Int& color);
extern void DissolveSelectedVertices(EditorMesh* em);
extern void DissolveSelectedEdges(EditorMesh* em);
extern void DissolveSelectedFaces(EditorMesh* em);
extern void SetHeight(EditorMesh* em, int index, float height);
extern int SplitEdge(EditorMesh* em, int edge_index, float edge_pos);
extern int SplitTriangle(EditorMesh* em, int triangle_index, const Vec2& position);
extern int AddVertex(EditorMesh* em, const Vec2& position);
extern int RotateEdge(EditorMesh* em, int edge_index);
extern void DrawMesh(EditorMesh* em, const Mat3& transform);
extern bool IsVertexOnOutsideEdge(EditorMesh* em, int v0);
extern Vec2 GetFaceCenter(EditorMesh* em, int face_index);
extern void UpdateEdges(EditorMesh* em);
extern void Center(EditorMesh* em);
extern int GetOrAddEdge(EditorMesh* em, int v0, int v1, int face_index);
extern bool FixWinding(EditorMesh* em, EditorFace& ef);
extern void DrawEdges(EditorMesh* em, const Vec2& position);
extern void DrawSelectedEdges(EditorMesh* em, const Vec2& position);
extern void DrawSelectedFaces(EditorMesh* em, const Vec2& position);
extern void DrawFaceCenters(EditorMesh* em, const Vec2& position);
extern void TriangulateFace(EditorMesh* em, EditorFace* ef, MeshBuilder* builder);
extern void DissolveEdge(EditorMesh* em, int edge_index);
extern int CreateFace(EditorMesh* em);
