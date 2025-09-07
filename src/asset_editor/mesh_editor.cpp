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
};

static MeshEditor g_mesh_editor = {};

extern void DrawEdges(const EditableAsset& ea, int min_edge_count, float zoom_scale, Color color);

static void DrawVertices(const EditableAsset& ea, bool selected)
{
    const EditableMesh& em = *ea.mesh;
    for (int i=0; i<em.vertex_count; i++)
    {
        const EditableVertex& ev = em.vertices[i];
        if (ev.selected != selected)
            continue;
        BindTransform(TRS(ev.position + ea.position, 0, VEC2_ONE * g_asset_editor.zoom_ref_scale * VERTEX_SIZE));
        DrawMesh(g_asset_editor.vertex_mesh);
    }
}

void UpdateMeshEditor(EditableAsset& ea)
{
    if (WasButtonPressed(g_asset_editor.input, MOUSE_LEFT))
    {
        int vertex_index = HitTestVertex(
            *ea.mesh,
            ScreenToWorld(g_asset_editor.camera, GetMousePosition()) - ea.position,
            VERTEX_SIZE * g_asset_editor.zoom_ref_scale);

        if (vertex_index != -1)
            ea.mesh->vertices[vertex_index].selected = true;
    }
}

void RenderMeshEditor(EditableAsset& ea)
{
    DrawEdges(ea, 10000, g_asset_editor.zoom_ref_scale, COLOR_EDGE);
    BindColor(COLOR_VERTEX);
    DrawVertices(ea, false);

    BindColor(COLOR_SELECTED);
    DrawVertices(ea, true);
}

void HandleMeshEditorBoxSelect(EditableAsset& ea, const Bounds2& bounds)
{
    EditableMesh& em = *ea.mesh;
    for (int i=0; i<em.vertex_count; i++)
    {
        EditableVertex& ev = em.vertices[i];
        Vec2 vpos = ev.position + ea.position;
        ev.selected =
            vpos.x >= bounds.min.x && vpos.x <= bounds.max.x &&
            vpos.y >= bounds.min.y && vpos.y <= bounds.max.y;
    }
}

void InitMeshEditor(EditableAsset& ea)
{
    for (int i=0; i<ea.mesh->vertex_count; i++)
        ea.mesh->vertices[i].selected = false;
}

