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
constexpr int MAX_FACE_VERTICES = 128;

constexpr int MESH_MAX_VERTICES = 1024;
constexpr int MESH_MAX_INDICES = 1024;
constexpr int MESH_MAX_FACES = 256;
constexpr int MESH_MAX_EDGES = 2048;
constexpr int MESH_MAX_TAGS = 8;

struct VertexWeight {
    int bone_index;
    float weight;
};

struct VertexData {
    Vec2 position;
    Vec2 edge_normal;
    float edge_size;
    bool selected;
    int ref_count;
    float gradient;
    VertexWeight weights[MESH_MAX_VERTEX_WEIGHTS];
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
    Vec3 normal;
    Vec2 center;
    bool selected;
    int vertices[MAX_FACE_VERTICES];
    int vertex_count;
};

struct TagData {
    Vec2 position;
    float rotation;
    const Name* name;
    VertexWeight weights[MESH_MAX_VERTEX_WEIGHTS];
};

struct MeshRuntimeData {
    VertexData vertices[MESH_MAX_VERTICES];
    EdgeData edges[MESH_MAX_EDGES];
    FaceData faces[MESH_MAX_FACES];
    TagData tags[MESH_MAX_TAGS];
    int face_vertices[MESH_MAX_INDICES];
};

struct MeshData : AssetData {
    MeshRuntimeData* data;
    VertexData* vertices;
    EdgeData* edges;
    FaceData* faces;
    TagData* tags;

    int vertex_count;
    int edge_count;
    int face_count;
    int tag_count;
    int selected_vertex_count;
    int selected_edge_count;
    int selected_face_count;
    Mesh* mesh;
    Mesh* outline;
    int outline_version;
    Vec2Int edge_color;
    int depth;
    int hold;
};

extern void InitMeshData(AssetData* a);
extern AssetData* NewMeshData(const std::filesystem::path& path);
extern MeshData* Clone(Allocator* allocator, MeshData* m);
extern MeshData* LoadEditorMesh(const std::filesystem::path& path);
extern Mesh* ToMesh(MeshData* m, bool upload=true, bool use_cache=true);
extern Mesh* ToOutlineMesh(MeshData* m);
extern int HitTestFace(MeshData* m, const Mat3& transform, const Vec2& position);
extern int HitTestFaces(MeshData* m, const Mat3& transform, const Vec2& position, int* faces, int max_faces=MAX_FACES);
extern int HitTestVertex(MeshData* m, const Mat3& transform, const Vec2& position, float size_mult=1.0f);
inline int HitTestVertex(MeshData* m, const Vec2& position, float size_mult=1.0f) {
    return HitTestVertex(m, Translate(m->position), position, size_mult);
}
extern int HitTestVertex(const Vec2& position, const Vec2& hit_pos, float size_mult=1.0f);
extern int HitTestEdge(MeshData* m, const Mat3& transform, const Vec2& position, float* where=nullptr, float size_mult=1.0f);
inline int HitTestEdge(MeshData* m, const Vec2& position, float* where=nullptr, float size_mult=1.0f) {
    return HitTestEdge(m, Translate(m->position), position, where, size_mult);
}
extern int HitTestTag(MeshData* m, const Vec2& position, float size_mult=1.0f);
extern Vec2 HitTestSnap(MeshData* m, const Vec2& position);
extern void AddTag(MeshData* m, const Vec2& position);
extern void RemoveTag(MeshData* m, int index);
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
extern void DrawMesh(MeshData* m, const Mat3& transform, Material* material=nullptr);
extern bool IsVertexOnOutsideEdge(MeshData* m, int v0);
extern Vec2 GetFaceCenter(MeshData* m, int face_index);
extern Vec2 GetEdgePoint(MeshData* m, int edge_index, float t);
inline Vec2 GetVertexPoint(MeshData* m, int vertex_index) {
    return m->vertices[vertex_index].position;
}
extern void UpdateEdges(MeshData* m);
extern void Center(MeshData* m);
extern int GetEdge(MeshData* m, int v0, int v1);
extern int GetOrAddEdge(MeshData* m, int v0, int v1, int face_index);
extern bool FixWinding(MeshData* m, FaceData& ef);
extern void DrawEdges(MeshData* m, const Vec2& position);
extern void DrawEdges(MeshData* m, const Mat3& transform);
extern void DrawSelectedEdges(MeshData* m, const Vec2& position);
extern void DrawSelectedFaces(MeshData* m, const Vec2& position);
extern void DrawFaceCenters(MeshData* m, const Vec2& position);
extern void DissolveEdge(MeshData* m, int edge_index);
extern int CreateFace(MeshData* m);
extern int GetSelectedVertices(MeshData* m, int vertices[MAX_VERTICES]);
extern int GetSelectedEdges(MeshData* m, int edges[MAX_EDGES]);
extern void SerializeMesh(Mesh* m, Stream* stream);
extern void SwapFace(MeshData* m, int face_index_a, int face_index_b);
extern void SetOrigin(MeshData* m, const Vec2& origin);
extern float GetVertexWeight(MeshData* m, int vertex_index, int bone_index);
extern void AddVertexWeight(MeshData* m, int vertex_index, int bone_index, float weight);
extern void SetVertexWeight(MeshData* m, int vertex_index, int bone_index, float weight);
