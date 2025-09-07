//
//  MeshZ - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"
#include "tui/text_input.h"

extern int HitTestVertex(const EditableMesh& em, const Vec2& world_pos, float dist);
extern Vec2 SnapToGrid(const Vec2& position, bool secondary);

constexpr float VERTEX_SIZE = 0.1f;
constexpr float CENTER_SIZE = 0.05f;
constexpr Color COLOR_EDGE = { 0.25f, 0.25f, 0.25f, 1.0f};
constexpr Color COLOR_VERTEX = { 0.95f, 0.95f, 0.95f, 1.0f};
constexpr Color COLOR_CENTER = { 1, 0, 0, 1};

enum MeshEditorState
{
    MESH_EDITOR_STATE_NONE,
    MESH_EDITOR_STATE_MOVE,
    MESH_EDITOR_STATE_ROTATE,
    MESH_EDITOR_STATE_SCALE,
};

struct MeshEditor
{
    MeshEditorState state;
    i32 selected_vertex_count;
    Vec2 world_drag_start;
    Vec2 selection_drag_start;
    Vec2 selection_center;
    Material* color_material;
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

static void UpdateSelection(EditableAsset& ea)
{
    EditableMesh& em = *ea.mesh;
    Vec2 center = VEC2_ZERO;
    int count = 0;
    for (int i=0; i<em.vertex_count; i++)
    {
        const EditableVertex& ev = em.vertices[i];
        if (!ev.selected)
            continue;

        center += ev.position;
        count++;
    }

    if (count > 0)
        center = center * (1.0f / count);

    g_mesh_editor.selected_vertex_count = count;
    g_mesh_editor.selection_center = center;
}

static void RevertPositions(EditableAsset& ea)
{
    EditableMesh& em = *ea.mesh;
    for (int i=0; i<em.vertex_count; i++)
    {
        EditableVertex& ev = em.vertices[i];
        ev.position = ev.saved_position;
    }

    ea.dirty = true;
    UpdateSelection(ea);
}

static void UpdateScaleState(EditableAsset& ea)
{
    float delta =
        Length(g_asset_editor.world_mouse_position - g_mesh_editor.selection_drag_start) -
        Length(g_mesh_editor.world_drag_start - g_mesh_editor.selection_drag_start);

    EditableMesh& em = *ea.mesh;
    for (i32 i=0; i<em.vertex_count; i++)
    {
        EditableVertex& ev = em.vertices[i];
        if (!ev.selected)
            continue;

        Vec2 dir = ev.saved_position - g_mesh_editor.selection_center;
        ev.position = g_mesh_editor.selection_center + dir * (1.0f + delta);
    }

    em.dirty = true;

    if (WasButtonPressed(g_asset_editor.input, MOUSE_LEFT))
    {
        UpdateSelection(ea);
        g_mesh_editor.state = MESH_EDITOR_STATE_NONE;
    }
    else if (WasButtonPressed(g_asset_editor.input, MOUSE_RIGHT))
    {
        RevertPositions(ea);
        g_mesh_editor.state = MESH_EDITOR_STATE_NONE;
    }
}

static void UpdateMoveState(EditableAsset& ea)
{
    Vec2 delta = g_asset_editor.world_mouse_position - g_mesh_editor.world_drag_start;
    Vec2 new_center = g_mesh_editor.selection_drag_start + delta;
    if (IsButtonDown(g_asset_editor.input, KEY_LEFT_CTRL))
        new_center = SnapToGrid(new_center, true);

    EditableMesh& em = *ea.mesh;
    for (int i=0; i<em.vertex_count; i++)
    {
        EditableVertex& ev = em.vertices[i];
        if (ev.selected)
            ev.position = ev.saved_position + delta;
    }

    em.dirty = true;
}

static void ClearVertexSelection(EditableAsset& ea)
{
    EditableMesh& em = *ea.mesh;
    for (int i=0; i<em.vertex_count; i++)
        em.vertices[i].selected = false;

    g_mesh_editor.selected_vertex_count = 0;
}

static void SetState(EditableAsset& ea, MeshEditorState state)
{
    g_mesh_editor.state = state;
    g_mesh_editor.world_drag_start = g_asset_editor.world_mouse_position;
    g_mesh_editor.selection_drag_start = ea.position + g_mesh_editor.selection_center;

    for (int i=0; i<ea.mesh->vertex_count; i++)
    {
        EditableVertex& ev = ea.mesh->vertices[i];
        ev.saved_position = ev.position;
    }
}

void UpdateMeshEditor(EditableAsset& ea)
{
    BeginCanvas(UI_REF_WIDTH, UI_REF_HEIGHT);
    SetStyleSheet(g_assets.ui.mesh_editor);
    Image(g_mesh_editor.color_material, GetName("color_picker_image"));
    EndCanvas();

    switch (g_mesh_editor.state)
    {
    case MESH_EDITOR_STATE_NONE:
        // Single select vertex
        if (WasButtonPressed(g_asset_editor.input, MOUSE_LEFT))
        {
            int vertex_index = HitTestVertex(
                *ea.mesh,
                ScreenToWorld(g_asset_editor.camera, GetMousePosition()) - ea.position,
                VERTEX_SIZE * g_asset_editor.zoom_ref_scale);

            if (!IsButtonDown(g_asset_editor.input, KEY_LEFT_CTRL))
            {
                ClearVertexSelection(ea);
                ea.mesh->vertices[vertex_index].selected = true;
            }
            else if (ea.mesh->vertices[vertex_index].selected)
                ea.mesh->vertices[vertex_index].selected = false;
            else
                ea.mesh->vertices[vertex_index].selected = true;

            UpdateSelection(ea);
        }

        // Move mode
        if (g_mesh_editor.state == MESH_EDITOR_STATE_NONE &&
            WasButtonPressed(g_asset_editor.input, KEY_G) &&
            g_mesh_editor.selected_vertex_count > 0)
        {
            SetState(ea, MESH_EDITOR_STATE_MOVE);
        }
        // Scale mode
        else if (g_mesh_editor.state == MESH_EDITOR_STATE_NONE &&
            WasButtonPressed(g_asset_editor.input, KEY_S) &&
            g_mesh_editor.selected_vertex_count > 0)
        {
            SetState(ea, MESH_EDITOR_STATE_SCALE);
        }

        return;

    case MESH_EDITOR_STATE_MOVE:
    {
        UpdateMoveState(ea);
        break;
    }

    case MESH_EDITOR_STATE_SCALE:
    {
        UpdateScaleState(ea);
        break;
    }

    default:
        break;
    }

    // Commit the tool
    if (WasButtonPressed(g_asset_editor.input, MOUSE_LEFT) || WasButtonPressed(g_asset_editor.input, KEY_ENTER))
    {
        UpdateSelection(ea);
        g_mesh_editor.state = MESH_EDITOR_STATE_NONE;
    }
    // Cancel the tool
    else if (WasButtonPressed(g_asset_editor.input, KEY_ESCAPE) || WasButtonPressed(g_asset_editor.input, MOUSE_RIGHT))
    {
        RevertPositions(ea);
        g_mesh_editor.state = MESH_EDITOR_STATE_NONE;
    }
}

void RenderMeshEditor(EditableAsset& ea)
{
    DrawEdges(ea, 10000, g_asset_editor.zoom_ref_scale, COLOR_EDGE);
    BindColor(COLOR_VERTEX);
    DrawVertices(ea, false);

    BindColor(COLOR_SELECTED);
    DrawVertices(ea, true);

    BindColor(COLOR_CENTER);
    BindTransform(TRS(ea.position, 0, VEC2_ONE * g_asset_editor.zoom_ref_scale * CENTER_SIZE));
    DrawMesh(g_asset_editor.vertex_mesh);
}

void HandleMeshEditorBoxSelect(EditableAsset& ea, const Bounds2& bounds)
{
    EditableMesh& em = *ea.mesh;
    g_mesh_editor.selected_vertex_count = 0;
    for (int i=0; i<em.vertex_count; i++)
    {
        EditableVertex& ev = em.vertices[i];
        Vec2 vpos = ev.position + ea.position;
        ev.selected =
            vpos.x >= bounds.min.x && vpos.x <= bounds.max.x &&
            vpos.y >= bounds.min.y && vpos.y <= bounds.max.y;

        if (ev.selected)
            g_mesh_editor.selected_vertex_count++;
    }

    UpdateSelection(ea);
}

void InitMeshEditor(EditableAsset& ea)
{
    g_mesh_editor.state = MESH_EDITOR_STATE_NONE;

    for (int i=0; i<ea.mesh->vertex_count; i++)
        ea.mesh->vertices[i].selected = false;

    if (!g_mesh_editor.color_material)
    {
        g_mesh_editor.color_material = CreateMaterial(ALLOCATOR_DEFAULT, g_core_assets.shaders.ui);
        SetTexture(g_mesh_editor.color_material, g_assets.textures.palette, 0);
    }
}

