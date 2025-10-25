//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

constexpr int MAX_VERTICES = 1024;
constexpr int MAX_FACES = MAX_VERTICES / 3;
constexpr int MAX_INDICES = MAX_FACES * 3;
constexpr int MAX_EDGES = MAX_VERTICES * 2;

struct VertexData {
    Vec2 position;
    float height;
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
};

inline MeshData* GetMeshData(int index) {
    AssetData* ea = GetAssetData(index);
    if (!ea)
        return nullptr;

    assert(ea->type == ASSET_TYPE_MESH);
    return (MeshData*)ea;
}

extern void InitEditorMesh(AssetData* a);
extern AssetData* NewEditorMesh(const std::filesystem::path& path);
extern MeshData* Clone(Allocator* allocator, MeshData* em);
extern MeshData* LoadEditorMesh(const std::filesystem::path& path);
extern Mesh* ToMesh(MeshData* em, bool upload=true);
extern int HitTestFace(MeshData* em, const Vec2& position, const Vec2& hit_pos, Vec2* where = nullptr);
extern int HitTestVertex(MeshData* em, const Vec2& world_pos, float size_mult=1.0f);
extern int HitTestEdge(MeshData* em, const Vec2& hit_pos, float* where=nullptr);
extern Bounds2 GetSelectedBounds(MeshData* em);
extern void MarkDirty(MeshData* em);
extern void SetSelectedTrianglesColor(MeshData* em, const Vec2Int& color);
extern void SetEdgeColor(MeshData* em, const Vec2Int& color);
extern void DissolveSelectedVertices(MeshData* em);
extern void DissolveSelectedEdges(MeshData* em);
extern void DissolveSelectedFaces(MeshData* em);
extern void SetHeight(MeshData* em, int index, float height);
extern int SplitEdge(MeshData* em, int edge_index, float edge_pos);
extern int SplitTriangle(MeshData* em, int triangle_index, const Vec2& position);
extern int AddVertex(MeshData* em, const Vec2& position);
extern int RotateEdge(MeshData* em, int edge_index);
extern void DrawMesh(MeshData* em, const Mat3& transform);
extern bool IsVertexOnOutsideEdge(MeshData* em, int v0);
extern Vec2 GetFaceCenter(MeshData* em, int face_index);
extern void UpdateEdges(MeshData* em);
extern void Center(MeshData* em);
extern int GetOrAddEdge(MeshData* em, int v0, int v1, int face_index);
extern bool FixWinding(MeshData* em, FaceData& ef);
extern void DrawEdges(MeshData* em, const Vec2& position);
extern void DrawSelectedEdges(MeshData* em, const Vec2& position);
extern void DrawSelectedFaces(MeshData* em, const Vec2& position);
extern void DrawFaceCenters(MeshData* em, const Vec2& position);
extern void TriangulateFace(MeshData* em, FaceData* ef, MeshBuilder* builder);
extern void DissolveEdge(MeshData* em, int edge_index);
extern int CreateFace(MeshData* em);
