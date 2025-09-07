//
//  MeshZ - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"

extern int HitTestVertex(const EditableMesh& em, const Vec2& world_pos, float dist);

constexpr float VERTEX_SIZE = 0.1f;

constexpr Color COLOR_EDGE = { 0.25f, 0.25f, 0.25f, 1.0f};
constexpr Color COLOR_VERTEX = { 0.95f, 0.95f, 0.95f, 1.0f};

struct MeshEditor
{
    i32 selected_vertex;
};

static MeshEditor g_mesh_editor = {};

extern void DrawEdges(const EditableAsset& ea, int min_edge_count, float zoom_scale, Color color);

static void DrawVertex(const EditableAsset& ea, int vertex_index)
{
    const EditableMesh& em = *ea.mesh;
    const EditableVertex& ev = em.vertices[vertex_index];
    BindTransform(TRS(ev.position + ea.position, 0, VEC2_ONE * g_asset_editor.zoom_ref_scale * VERTEX_SIZE));
    DrawMesh(g_asset_editor.vertex_mesh);
}

static void DrawVertices(const EditableAsset& ea)
{
    const EditableMesh& em = *ea.mesh;
    for (int i=0; i<em.vertex_count; i++)
        DrawVertex(ea, i);
}

void UpdateMeshEditor(EditableAsset& ea)
{
    if (WasButtonPressed(g_asset_editor.input, MOUSE_LEFT))
    {
        g_mesh_editor.selected_vertex = HitTestVertex(
            *ea.mesh,
            ScreenToWorld(g_asset_editor.camera, GetMousePosition()) - ea.position,
            VERTEX_SIZE * g_asset_editor.zoom_ref_scale);
    }
}

void RenderMeshEditor(EditableAsset& ea)
{
    DrawEdges(ea, 10000, g_asset_editor.zoom_ref_scale, COLOR_EDGE);
    BindColor(COLOR_VERTEX);
    DrawVertices(ea);

    if (g_mesh_editor.selected_vertex != -1)
    {
        BindColor(COLOR_SELECTED);
        DrawVertex(ea, g_mesh_editor.selected_vertex);
    }
}

void InitMeshEditor(EditableAsset& ea)
{
    g_mesh_editor.selected_vertex = -1;
}