//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

constexpr int MAX_VERTICES = 1024;
constexpr int MAX_FACES = MAX_VERTICES / 3;
constexpr int MAX_INDICES = MAX_FACES * 3;
constexpr int MAX_EDGES = MAX_VERTICES * 2;
constexpr int MIN_DEPTH = 0;
constexpr int MAX_DEPTH = 100;

struct VertexData {
    Vec2 position;
    Vec2 edge_normal;
    float edge_size;
    bool selected;
    int ref_count;
    float gradient;
};

struct EdgeData {
    int v0;
    int v1;
    int face_count;
    int face_index[2];
    Vec2 normal;
    bool selected;
};

struct FaceData {
    Vec2Int color;
    Vec2Int gradient_color;
    Vec2 gradient_dir;
    float gradient_offset;
    Vec3 normal;
    bool selected;
    int vertex_offset;
    int vertex_count;
};

struct MeshData : AssetData {
    VertexData vertices[MAX_VERTICES];
    EdgeData edges[MAX_EDGES];
    FaceData faces[MAX_FACES];
    int face_vertices[MAX_INDICES];
    int face_vertex_count;
    int vertex_count;
    int edge_count;
    int face_count;
    int selected_count;
    Mesh* mesh;
    Vec2Int edge_color;
    float opacity;
    int depth;
};

extern void InitEditorMesh(AssetData* a);
extern AssetData* NewMeshData(const std::filesystem::path& path);
extern MeshData* Clone(Allocator* allocator, MeshData* m);
extern MeshData* LoadEditorMesh(const std::filesystem::path& path);
extern Mesh* ToMesh(MeshData* m, bool upload=true);
extern int HitTestFace(MeshData* m, const Mat3& transform, const Vec2& hit_pos, Vec2* where = nullptr);
extern int HitTestVertex(MeshData* m, const Vec2& world_pos, float size_mult=1.0f);
extern int HitTestEdge(MeshData* m, const Vec2& hit_pos, float* where=nullptr);
extern Bounds2 GetSelectedBounds(MeshData* m);
extern void MarkDirty(MeshData* m);
extern void SetSelectedTrianglesColor(MeshData* m, const Vec2Int& color);
extern void SetEdgeColor(MeshData* m, const Vec2Int& color);
extern void DissolveSelectedVertices(MeshData* m);
extern void DissolveSelectedEdges(MeshData* m);
extern void DissolveSelectedFaces(MeshData* m);
extern void SetHeight(MeshData* m, int index, float height);
extern int SplitEdge(MeshData* m, int edge_index, float edge_pos, bool update=true);
extern int SplitTriangle(MeshData* m, int triangle_index, const Vec2& position);
extern int AddVertex(MeshData* m, const Vec2& position);
extern int RotateEdge(MeshData* m, int edge_index);
extern void DrawMesh(MeshData* m, const Mat3& transform);
extern bool IsVertexOnOutsideEdge(MeshData* m, int v0);
extern Vec2 GetFaceCenter(MeshData* m, int face_index);
extern void UpdateEdges(MeshData* m);
extern void Center(MeshData* m);
extern int GetOrAddEdge(MeshData* m, int v0, int v1, int face_index);
extern bool FixWinding(MeshData* m, FaceData& ef);
extern void DrawEdges(MeshData* m, const Vec2& position);
extern void DrawEdges(MeshData* m, const Mat3& transform);
extern void DrawSelectedEdges(MeshData* m, const Vec2& position);
extern void DrawSelectedFaces(MeshData* m, const Vec2& position);
extern void DrawFaceCenters(MeshData* m, const Vec2& position);
extern void DissolveEdge(MeshData* m, int edge_index);
extern int CreateFace(MeshData* m);

extern int GetSelectedEdges(MeshData* m, int edges[MAX_EDGES]);
