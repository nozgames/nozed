//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

struct EditableVertex
{
    Vec2 position;
    Vec2 saved_position;
    float height;
    float saved_height;
    bool selected;
    Vec2 edge_normal;
};

struct EditableEdge
{
    int v0;
    int v1;
    int triangle_count;
    Vec2 normal;
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
    Mesh* mesh;
    int vertex_count;
    int edge_count;
    int triangle_count;
    bool dirty;
    Bounds2 bounds;
    bool modified;
    int selected_vertex_count;
};

// @editable_mesh
extern EditableMesh* CreateEditableMesh(Allocator* allocator);
extern bool HitTest(const EditableMesh& mesh, const Vec2& position, const Bounds2& hit_bounds);
extern bool HitTestTriangle(const EditableMesh& em, const EditableTriangle& et, const Vec2& position, const Vec2& hit_pos, Vec2* where = nullptr);
extern int HitTestTriangle(const EditableMesh& mesh, const Vec2& position, const Vec2& hit_pos, Vec2* where = nullptr);
extern int HitTestEdge(const EditableMesh& em, const Vec2& hit_pos, float size, float* where=nullptr);
extern Mesh* ToMesh(EditableMesh& em);
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
extern EditableMesh* Clone(Allocator* allocator, const EditableMesh& em);
extern void Copy(EditableMesh& dst, const EditableMesh& src);
