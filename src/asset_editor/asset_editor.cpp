//
//  MeshZ - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"
#include "file_helpers.h"

constexpr float VERTEX_SIZE = 0.1f;
constexpr float REF_ZOOM = 10.0f;
constexpr Color VERTEX_COLOR = { 0.95f, 0.95f, 0.95f, 1.0f};
constexpr Color VIEW_COLOR = {0.05f, 0.05f, 0.05f, 1.0f};

extern View* CreateView(Allocator* allocator);
extern void InitGrid(Allocator* allocator);
extern void ShutdownGrid();

extern EditableMesh* CreateEditableMesh(Allocator* allocator);
extern Mesh* ToMesh(EditableMesh* emesh);
extern void DrawVertexHandles(EditableMesh* mesh);
extern void SetPosition(EditableMesh* emesh, int index, const Vec2& position);
extern int SplitEdge(EditableMesh* emesh, int edge_index, float edge_pos);
extern void DeleteVertex(EditableMesh* mesh, int vertex_index);
extern void DissolveVertex(EditableMesh* mesh, int vertex_index);
extern void RotateEdge(EditableMesh* mesh, int edge_index);
extern bool SaveEditableMesh(const EditableMesh* mesh, const char* filename);
extern void SetTriangleColor(EditableMesh* emesh, int index, const Vec2Int& color);
extern Vec2 SnapToGrid(const Vec2& position, bool secondary);
extern i32 LoadEditableAssets(EditableAsset** assets);
extern int HitTestVertex(const EditableMesh& em, const Vec2& world_pos, float dist);
extern int HitTestAssets(const Vec2& hit_pos);
extern void MoveTo(EditableAsset& asset, const Vec2& position);
extern void SaveAssetMetaData();
extern void UpdateMeshEditor(EditableAsset& ea);
extern void DrawEdges(const EditableAsset& ea, int min_edge_count, float zoom_scale, Color color);
extern void InitMeshEditor(EditableAsset& ea);
extern void RenderMeshEditor(EditableAsset& ea);

AssetEditor g_asset_editor = {};

#if 0
static int HitTestVertex()
{
    Vec2 mouse = GetMousePosition();
    Vec2 mouse_world = ScreenToWorld(g_asset_editor.camera, mouse);
    return HitTestVertex(g_asset_editor.emesh, mouse_world, VERTEX_SIZE * g_asset_editor.zoom / REF_ZOOM);
}

static int HitTestTriangle()
{
    Vec2 mouse = GetMousePosition();
    Vec2 mouse_world = ScreenToWorld(g_asset_editor.camera, mouse);

    for (int i=0; i<g_asset_editor.emesh->triangle_count; i++)
    {
        EditableTriangle& et = g_asset_editor.emesh->triangles[i];
        Vec2 v0 = g_asset_editor.emesh->vertices[et.v0].position;
        Vec2 v1 = g_asset_editor.emesh->vertices[et.v1].position;
        Vec2 v2 = g_asset_editor.emesh->vertices[et.v2].position;

        // Barycentric technique
        float area = 0.5f *(-v1.y * v2.x + v0.y * (-v1.x + v2.x) + v0.x * (v1.y - v2.y) + v1.x * v2.y);
        float s = 1/(2*area)*(v0.y*v2.x - v0.x*v2.y + (v2.y - v0.y)*mouse_world.x + (v0.x - v2.x)*mouse_world.y);
        float t = 1/(2*area)*(v0.x*v1.y - v0.y*v1.x + (v0.y - v1.y)*mouse_world.x + (v1.x - v0.x)*mouse_world.y);

        if (s >= 0 && t >= 0 && (s + t) <= 1)
            return i;
    }

    return -1;
}

static int HitTestEdge(float* pos)
{
    Vec2 mouse = GetMousePosition();
    Vec2 mouse_world = ScreenToWorld(g_asset_editor.camera, mouse);
    float zoom_scale = g_asset_editor.zoom / REF_ZOOM;

    for (int i=0; i<g_asset_editor.emesh->edge_count; i++)
    {
        EditableEdge& ee = g_asset_editor.emesh->edges[i];
        Vec2 v0 = g_asset_editor.emesh->vertices[ee.v0].position;
        Vec2 v1 = g_asset_editor.emesh->vertices[ee.v1].position;
        Vec2 edge_dir = Normalize(v1 - v0);
        Vec2 to_mouse = mouse_world - v0;
        float edge_length = Length(v1 - v0);
        float proj = Dot(to_mouse, edge_dir);
        if (proj >= 0 && proj <= edge_length)
        {
            Vec2 closest_point = v0 + edge_dir * proj;
            float dist = Length(mouse_world - closest_point);
            if (dist < VERTEX_SIZE * 0.5f * zoom_scale)
            {
                if (pos)
                    *pos = proj / edge_length;
                return i;
            }
        }
    }

    return -1;
}
#endif

static void UpdateCamera()
{
    SetExtents(g_asset_editor.camera, F32_MIN, F32_MIN, -g_asset_editor.zoom * 0.5f, g_asset_editor.zoom * 0.5f);
}

static void FrameView(int asset_index)
{
    const EditableAsset* ea = g_asset_editor.assets[asset_index];
    if (ea->type != EDITABLE_ASSET_TYPE_MESH)
        return;

    const EditableMesh& em = *ea->mesh;

    // Calculate the center of the mesh bounds
    Vec2 center = Vec2{
        (em.bounds.min.x + em.bounds.max.x) * 0.5f,
        (em.bounds.min.y + em.bounds.max.y) * 0.5f
    };

    // Calculate the size of the mesh bounds
    Vec2 size = Vec2{
        em.bounds.max.x - em.bounds.min.x,
        em.bounds.max.y - em.bounds.min.y
    };

    // Calculate appropriate zoom level to fit mesh at 75% of screen
    constexpr float TARGET_FILL_PERCENTAGE = 0.75f;
    float scale = 1.0f / TARGET_FILL_PERCENTAGE;
    
    // Choose the larger dimension to ensure the entire mesh fits within 75% of screen
    float max_dimension = Max(size.x, size.y);
    
    // Ensure minimum size to avoid division by zero
    constexpr float MIN_DIMENSION = 0.1f;
    if (max_dimension < MIN_DIMENSION) max_dimension = MIN_DIMENSION;
    
    // Calculate zoom level that will fit the mesh at 75% of screen
    // The camera extent is zoom * 0.5, so zoom = extent * 2
    float desired_extent = max_dimension * scale * 0.5f;
    g_asset_editor.zoom = desired_extent * 2.0f;
    
    // Set camera position and update camera extents
    SetPosition(g_asset_editor.camera, center + ea->position);
    UpdateCamera();
}

static bool s_panning = false;
static Vec2 s_pan_start_mouse;
static Vec2 s_pan_start_camera;

static void PanView()
{
    // Start panning when space + mouse button are both pressed
    if (IsButtonDown(g_asset_editor.input, KEY_SPACE) && WasButtonPressed(g_asset_editor.input, MOUSE_LEFT))
    {
        s_panning = true;
        s_pan_start_mouse = GetMousePosition();
        
        // Get current camera bounds to extract position
        Bounds2 bounds = GetBounds(g_asset_editor.camera);
        s_pan_start_camera = Vec2{
            (bounds.min.x + bounds.max.x) * 0.5f,
            (bounds.min.y + bounds.max.y) * 0.5f
        };
    }
    
    // Continue panning while both space and mouse are held
    if (s_panning && IsButtonDown(g_asset_editor.input, KEY_SPACE) && IsButtonDown(g_asset_editor.input, MOUSE_LEFT))
    {
        // Calculate mouse delta in screen space, then convert to world space delta
        Vec2 mouse_delta = GetMousePosition() - s_pan_start_mouse;
        Vec2 world_delta_start = ScreenToWorld(g_asset_editor.camera, s_pan_start_mouse);
        Vec2 world_delta_current = ScreenToWorld(g_asset_editor.camera, s_pan_start_mouse + mouse_delta);
        Vec2 world_delta = world_delta_start - world_delta_current; // Invert for natural panning
        
        // Apply pan offset to camera position
        SetPosition(g_asset_editor.camera, s_pan_start_camera + world_delta);
    }
    
    // Stop panning when either space or mouse is released
    if (WasButtonReleased(g_asset_editor.input, KEY_SPACE) || WasButtonReleased(g_asset_editor.input, MOUSE_LEFT))
    {
        s_panning = false;
    }
}

void UpdateAssetEditor()
{
    if (!g_asset_editor.edit_mode)
        g_asset_editor.hover_asset = HitTestAssets(ScreenToWorld(g_asset_editor.camera, GetMousePosition()));

    if (WasButtonPressed(g_asset_editor.input, KEY_ESCAPE))
    {
        SaveAssetMetaData();
    }

    if (WasButtonPressed(g_asset_editor.input, KEY_TAB))
    {
        if (g_asset_editor.selected_asset != -1)
        {
            g_asset_editor.edit_mode = !g_asset_editor.edit_mode;
            if (g_asset_editor.edit_mode && g_asset_editor.selected_asset != -1)
            {
                EditableAsset& ea = *g_asset_editor.assets[g_asset_editor.selected_asset];
                switch (ea.type)
                {
                case EDITABLE_ASSET_TYPE_MESH:
                    InitMeshEditor(ea);
                    break;

                default:
                    break;
                }
            }
        }
    }

    if (WasButtonPressed(g_asset_editor.input, KEY_F))
    {
        if (g_asset_editor.selected_asset != -1)
            FrameView(g_asset_editor.selected_asset);
    }

    if (!g_asset_editor.edit_mode && WasButtonPressed(g_asset_editor.input, MOUSE_LEFT))
    {
        if (g_asset_editor.hover_asset != -1 && g_asset_editor.selected_asset == g_asset_editor.hover_asset )
        {
            g_asset_editor.dragging = true;
            g_asset_editor.drag_start = GetMousePosition();
            g_asset_editor.drag_position_start = g_asset_editor.assets[g_asset_editor.selected_asset]->position;
        }
        else
        {
            g_asset_editor.selected_asset = g_asset_editor.hover_asset;
        }

        // g_asset_editor.selected_vertex = HitTestVertex();
        // if (g_asset_editor.selected_vertex != -1)
        // {
        //     g_asset_editor.drag_start = GetMousePosition();
        //     g_asset_editor.drag_position_start = g_asset_editor.emesh->vertices[g_asset_editor.selected_vertex].position;
        // }
    }

    // if (WasButtonPressed(g_asset_editor.input, MOUSE_MIDDLE))
    // {
    //     int edge = HitTestEdge(nullptr);
    //     if (edge != -1)
    //         RotateEdge(g_asset_editor.emesh, edge);
    // }

    // if (WasButtonPressed(g_asset_editor.input, MOUSE_RIGHT))
    // {
    //     float edge_pos = 0.0f;
    //     int edge = HitTestEdge(&edge_pos);
    //
    //     if (edge != -1)
    //     {
    //         int new_vertex = SplitEdge(g_asset_editor.emesh, edge, edge_pos);
    //         if (new_vertex != -1)
    //         {
    //             g_asset_editor.selected_vertex = new_vertex;
    //             g_asset_editor.drag_start = GetMousePosition();
    //             g_asset_editor.drag_position_start = g_asset_editor.emesh->vertices[g_asset_editor.selected_vertex].position;
    //         }
    //     }
    // }

    if (!g_asset_editor.edit_mode && WasButtonReleased(g_asset_editor.input, MOUSE_LEFT) || WasButtonReleased(g_asset_editor.input, MOUSE_RIGHT))
    {
        g_asset_editor.dragging = false;
        g_asset_editor.selected_vertex = -1;
    }

    // if (WasButtonPressed(g_asset_editor.input, KEY_SPACE))
    // {
    //     int triangle = HitTestTriangle();
    //     if (triangle != -1)
    //     {
    //         SetTriangleColor(g_asset_editor.emesh, triangle, Vec2Int{g_asset_editor.emesh->triangles[triangle].color.x + 1, 1});
    //     }
    // }

    // if (WasButtonPressed(g_asset_editor.input, KEY_X))
    // {
    //     int vertex = HitTestVertex();
    //     if (vertex != -1)
    //         DissolveVertex(g_asset_editor.emesh, vertex);
    // }

    // if (g_asset_editor.selected_vertex != -1)
    // {
    //     Vec2 drag_delta =
    //         ScreenToWorld(g_asset_editor.camera, GetMousePosition()) - ScreenToWorld(g_asset_editor.camera, g_asset_editor.drag_start);
    //
    //     Vec2 world = g_asset_editor.drag_position_start + drag_delta;
    //     if (IsButtonDown(g_asset_editor.input, KEY_LEFT_CTRL))
    //         world = SnapToGrid(world, true);
    //
    //     SetPosition(g_asset_editor.emesh, g_asset_editor.selected_vertex, world);
    // }

    // Move selected asset
    if (g_asset_editor.dragging && g_asset_editor.selected_asset != -1)
    {
        Vec2 drag_delta =
            ScreenToWorld(g_asset_editor.camera, GetMousePosition()) - ScreenToWorld(g_asset_editor.camera, g_asset_editor.drag_start);

        Vec2 world = g_asset_editor.drag_position_start + drag_delta;
        if (IsButtonDown(g_asset_editor.input, KEY_LEFT_CTRL))
            world = SnapToGrid(world, true);

        MoveTo(*g_asset_editor.assets[g_asset_editor.selected_asset], world);
    }

    // Custom code for asset editor
    if (g_asset_editor.edit_mode && g_asset_editor.selected_asset != -1)
    {
        EditableAsset& ea = *g_asset_editor.assets[g_asset_editor.selected_asset];
        switch (ea.type)
        {
        case EDITABLE_ASSET_TYPE_MESH:
            UpdateMeshEditor(ea);
            break;

        default:
            break;
        }
    }

    // Panning (called every frame to handle press/hold/release)
    PanView();

    // Zoom in / out
    float zoom_axis = GetAxis(g_asset_editor.input, MOUSE_SCROLL_Y);
    if (zoom_axis < -0.5f || zoom_axis > 0.5f)
    {
        g_asset_editor.zoom -= zoom_axis;
        UpdateCamera();
    }
}

static void DrawMesh(int asset_index)
{
    const EditableAsset* ea = g_asset_editor.assets[asset_index];
    if (ea->type != EDITABLE_ASSET_TYPE_MESH)
        return;

    EditableMesh& em = *ea->mesh;
    BindTransform(TRS(ea->position, 0, VEC2_ONE));
    DrawMesh(ToMesh(&em));
}


void RenderView()
{
    BindCamera(g_asset_editor.camera);

    g_asset_editor.zoom_ref_scale = g_asset_editor.zoom / REF_ZOOM;

    // Draw grid first (behind everything else)
    DrawGrid(g_asset_editor.camera, g_asset_editor.zoom);

    BindColor(COLOR_WHITE);
    BindMaterial(g_asset_editor.material);
    for (int i=0; i<g_asset_editor.asset_count; i++)
        DrawMesh(i);


    // Draw edges
    BindTransform(MAT3_IDENTITY);

    if (g_asset_editor.edit_mode)
    {
        EditableAsset& ea = *g_asset_editor.assets[g_asset_editor.selected_asset];
        switch (ea.type)
        {
        case EDITABLE_ASSET_TYPE_MESH:
            RenderMeshEditor(ea);
            break;

        default:
            break;
        }
    }
    else if (g_asset_editor.selected_asset != -1)
    {
        EditableAsset& ea = *g_asset_editor.assets[g_asset_editor.selected_asset];
        DrawEdges(ea, 1, g_asset_editor.zoom_ref_scale, COLOR_SELECTED);
    }
}

int InitAssetEditor(int argc, const char* argv[])
{
    ApplicationTraits traits = {};
    Init(traits);
    traits.name = "meshz";
    traits.title = "MeshZ";
    traits.load_assets = LoadAssets;
    traits.unload_assets = UnloadAssets;
    traits.renderer.vsync = true;
    traits.assets_path = "build/assets";
    InitApplication(&traits);

    g_asset_editor.camera = CreateCamera(ALLOCATOR_DEFAULT);
    g_asset_editor.material = CreateMaterial(ALLOCATOR_DEFAULT, g_assets.shaders._default);
    g_asset_editor.vertex_material = CreateMaterial(ALLOCATOR_DEFAULT, g_assets.shaders.ui);
    g_asset_editor.zoom = 20.0f;
    g_asset_editor.selected_vertex = -1;
    g_asset_editor.selected_asset = -1;
    UpdateCamera();
    SetTexture(g_asset_editor.material, g_assets.textures.palette, 0);

    g_asset_editor.input = CreateInputSet(ALLOCATOR_DEFAULT);
    EnableButton(g_asset_editor.input, MOUSE_LEFT);
    EnableButton(g_asset_editor.input, MOUSE_RIGHT);
    EnableButton(g_asset_editor.input, MOUSE_MIDDLE);
    EnableButton(g_asset_editor.input, KEY_X);
    EnableButton(g_asset_editor.input, KEY_F);
    EnableButton(g_asset_editor.input, KEY_ESCAPE);
    EnableButton(g_asset_editor.input, KEY_SPACE);
    EnableButton(g_asset_editor.input, KEY_LEFT_CTRL);
    EnableButton(g_asset_editor.input, KEY_TAB);
    PushInputSet(g_asset_editor.input);

    MeshBuilder* builder = CreateMeshBuilder(ALLOCATOR_DEFAULT, 4, 6);
    AddVertex(builder, {   0, -0.5f}, {0,0,1}, VEC2_ZERO, 0);
    AddVertex(builder, { 0.5f, 0.0f}, {0,0,1}, VEC2_ZERO, 0);
    AddVertex(builder, {   0,  0.5f}, {0,0,1}, VEC2_ZERO, 0);
    AddVertex(builder, {-0.5f, 0.0f}, {0,0,1}, VEC2_ZERO, 0);
    AddTriangle(builder, 0, 1, 2);
    AddTriangle(builder, 2, 3, 0);
    g_asset_editor.vertex_mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE);

    Clear(builder);
    AddVertex(builder, { -1, -1});
    AddVertex(builder, {  1, -1});
    AddVertex(builder, {  1,  1});
    AddVertex(builder, { -1,  1});
    AddTriangle(builder, 0, 1, 2);
    AddTriangle(builder, 2, 3, 0);
    g_asset_editor.edge_mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE);

    Free(builder);

    InitGrid(ALLOCATOR_DEFAULT);

    g_asset_editor.asset_count = LoadEditableAssets(g_asset_editor.assets);

    // todo: read path from editor config
    while (UpdateApplication())
    {
        BeginUI();

        UpdateAssetEditor();

        EndUI();

        BeginRenderFrame(VIEW_COLOR);
        RenderView();
        DrawUI();
        EndRenderFrame();
    }

    ShutdownGrid();
    ShutdownApplication();

    return 0;
}
