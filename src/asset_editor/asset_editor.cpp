//
//  MeshZ - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"
#include "file_helpers.h"

constexpr float DEFAULT_DPI = 72.0f;
constexpr float ZOOM_MIN = 0.1f;
constexpr float ZOOM_MAX = 40.0f;
constexpr float ZOOM_STEP = 0.1f;
constexpr float ZOOM_DEFAULT = 1.0f;
constexpr float VERTEX_SIZE = 0.1f;
constexpr Color VERTEX_COLOR = { 0.95f, 0.95f, 0.95f, 1.0f};
constexpr Color VIEW_COLOR = {0.05f, 0.05f, 0.05f, 1.0f};
constexpr float BOX_SELECT_EDGE_WIDTH = 0.005f;
constexpr Color BOX_SELECT_COLOR = Color {0.2f, 0.6f, 1.0f, 0.025f};
constexpr Color BOX_SELECT_OUTLINE_COLOR = Color {0.2f, 0.6f, 1.0f, 0.2f};
constexpr float FRAME_VIEW_PERCENTAGE = 1.0f / 0.75f;

extern void SetPosition(EditableMesh* em, int index, const Vec2& position);
extern int SplitEdge(EditableMesh* em, int edge_index, float edge_pos);
extern void DeleteVertex(EditableMesh* em, int vertex_index);
extern void RotateEdge(EditableMesh* em, int edge_index);
extern void SetTriangleColor(EditableMesh* em, int index, const Vec2Int& color);
extern Vec2 SnapToGrid(const Vec2& position, bool secondary);

static bool IsEditing() { return g_asset_editor.state == ASSET_EDITOR_STATE_EDIT; }
static EditableAsset& GetEditingAsset() { return *g_asset_editor.assets[g_asset_editor.edit_asset_index]; }

AssetEditor g_asset_editor = {};

static void UpdateCamera()
{
    float DPI = g_asset_editor.dpi * g_asset_editor.ui_scale * g_asset_editor.zoom;
    Vec2Int screen_size = GetScreenSize();
    f32 world_width = screen_size.x / DPI;
    f32 world_height = screen_size.y / ((f32)screen_size.y * DPI / (f32)screen_size.y);
    f32 half_width = world_width * 0.5f;
    f32 half_height = world_height * 0.5f;
    SetExtents(g_asset_editor.camera, -half_width, half_width, -half_height, half_height, false);

    g_asset_editor.zoom_ref_scale = 1.0f / g_asset_editor.zoom;
}

static void FrameView()
{
    Bounds2 bounds = {};
    bool first = true;

    if (IsEditing())
    {
        EditableAsset& ea = GetEditingAsset();
        bounds = GetSelectedBounds(ea) + ea.position;
        first = GetSize(bounds) == VEC2_ZERO;
    }

    if (first)
    {
        for (int i=0; i<g_asset_editor.asset_count; i++)
        {
            const EditableAsset& ea = *g_asset_editor.assets[i];
            if (!ea.selected)
                continue;

            if (first)
                bounds = GetBounds(ea) + ea.position;
            else
                bounds = Union(bounds, GetBounds(ea) + ea.position);

            first = false;
        }
    }

    Vec2 center = GetCenter(bounds);
    Vec2 size = GetSize(bounds);

    f32 max_dimension = Max(size.x, size.y);
    if (max_dimension < ZOOM_MIN)
        max_dimension = ZOOM_MIN;
    
    Vec2Int screen_size = GetScreenSize();
    f32 target_world_height = max_dimension * FRAME_VIEW_PERCENTAGE;
    g_asset_editor.zoom = (f32)screen_size.y / (g_asset_editor.dpi * g_asset_editor.ui_scale * target_world_height);
    
    SetPosition(g_asset_editor.camera, center);
    UpdateCamera();
}

static void HandleBoxSelect()
{
    // When in edit mode let the editor handle it
    if (IsEditing())
    {
        EditableAsset& ea = *g_asset_editor.assets[g_asset_editor.edit_asset_index];
        switch (ea.type)
        {
        case EDITABLE_ASSET_TYPE_MESH:
        {
            HandleMeshEditorBoxSelect(ea, g_asset_editor.box_selection);
            break;
        }

        default:
            break;
        }

        return;
    }

    g_asset_editor.selected_asset_count = 0;
    for (int i=0; i<g_asset_editor.asset_count; i++)
    {
        EditableAsset& ea = *g_asset_editor.assets[i];
        ea.selected = HitTestAsset(ea, g_asset_editor.box_selection);
        if (ea.selected)
            g_asset_editor.selected_asset_count++;
    }
}

static void UpdateBoxSelect()
{
    if (g_asset_editor.panning)
        return;

    // Start tracking potential box selection on left mouse press
    if (!g_asset_editor.dragging && WasButtonPressed(g_asset_editor.input, MOUSE_LEFT))
    {
        g_asset_editor.box_start_mouse = GetMousePosition();
        g_asset_editor.box_start_world = ScreenToWorld(g_asset_editor.camera, g_asset_editor.box_start_mouse);

        // Initialize selection bounds but don't set box_selecting yet
        g_asset_editor.box_selection.min = g_asset_editor.box_start_world;
        g_asset_editor.box_selection.max = g_asset_editor.box_start_world;
    }
    
    // Check for meaningful drag distance and start box selection
    if (!g_asset_editor.box_selecting && IsButtonDown(g_asset_editor.input, MOUSE_LEFT))
    {
        Vec2 current_world = g_asset_editor.world_mouse_position;
        float width = Abs(current_world.x - g_asset_editor.box_start_world.x);
        float height = Abs(current_world.y - g_asset_editor.box_start_world.y);
        
        // Only start box selecting when drag is meaningful
        if (width > 0.01f || height > 0.01f)
        {
            g_asset_editor.box_selecting = true;
        }
    }
    
    // Continue box selection while dragging
    if (g_asset_editor.box_selecting && IsButtonDown(g_asset_editor.input, MOUSE_LEFT))
    {
        Vec2 current_world = g_asset_editor.world_mouse_position;
        
        // Update selection bounds
        g_asset_editor.box_selection.min = Vec2{
            Min(g_asset_editor.box_start_world.x, current_world.x),
            Min(g_asset_editor.box_start_world.y, current_world.y)
        };
        g_asset_editor.box_selection.max = Vec2{
            Max(g_asset_editor.box_start_world.x, current_world.x),
            Max(g_asset_editor.box_start_world.y, current_world.y)
        };
    }
    
    // End box selection on mouse release
    if (g_asset_editor.box_selecting && WasButtonReleased(g_asset_editor.input, MOUSE_LEFT))
    {
        g_asset_editor.box_selecting = false;
        HandleBoxSelect();
    }
}

static void PanView()
{
    // Start panning when space + mouse button are both pressed
    if (IsButtonDown(g_asset_editor.input, KEY_SPACE) && WasButtonPressed(g_asset_editor.input, MOUSE_LEFT))
    {
        g_asset_editor.panning = true;
        g_asset_editor.pan_start_mouse = GetMousePosition();

        // Get current camera bounds to extract position
        Bounds2 bounds = GetBounds(g_asset_editor.camera);
        g_asset_editor.pan_start_camera = Vec2{
            (bounds.min.x + bounds.max.x) * 0.5f,
            (bounds.min.y + bounds.max.y) * 0.5f
        };
    }

    // Continue panning while both space and mouse are held
    if (g_asset_editor.panning && IsButtonDown(g_asset_editor.input, KEY_SPACE) && IsButtonDown(g_asset_editor.input, MOUSE_LEFT))
    {
        // Calculate mouse delta in screen space, then convert to world space delta
        Vec2 mouse_delta = GetMousePosition() - g_asset_editor.pan_start_mouse;
        Vec2 world_delta_start = ScreenToWorld(g_asset_editor.camera, g_asset_editor.pan_start_mouse);
        Vec2 world_delta_current = ScreenToWorld(g_asset_editor.camera, g_asset_editor.pan_start_mouse + mouse_delta);
        Vec2 world_delta = world_delta_start - world_delta_current; // Invert for natural panning

        // Apply pan offset to camera position
        SetPosition(g_asset_editor.camera, g_asset_editor.pan_start_camera + world_delta);
    }

    // Stop panning when either space or mouse is released
    if (WasButtonReleased(g_asset_editor.input, KEY_SPACE) || WasButtonReleased(g_asset_editor.input, MOUSE_LEFT))
    {
        g_asset_editor.panning = false;
    }
}

static void ZoomView()
{
    float zoom_axis = GetAxis(g_asset_editor.input, MOUSE_SCROLL_Y);
    if (zoom_axis > -0.5f && zoom_axis < 0.5f)
        return;

    Vec2 mouse_screen = GetMousePosition();
    Vec2 world_under_cursor = ScreenToWorld(g_asset_editor.camera, mouse_screen);

    f32 zoom_factor = 1.0f + zoom_axis * ZOOM_STEP;
    g_asset_editor.zoom *= zoom_factor;
    g_asset_editor.zoom = Clamp(g_asset_editor.zoom, ZOOM_MIN, ZOOM_MAX);

    UpdateCamera();

    Vec2 new_screen_pos = WorldToScreen(g_asset_editor.camera, world_under_cursor);
    Vec2 screen_offset = mouse_screen - new_screen_pos;
    Vec2 world_offset = ScreenToWorld(g_asset_editor.camera, screen_offset) - ScreenToWorld(g_asset_editor.camera, VEC2_ZERO);
    Bounds2 bounds = GetBounds(g_asset_editor.camera);
    Vec2 current_center = Vec2{(bounds.min.x + bounds.max.x) * 0.5f, (bounds.min.y + bounds.max.y) * 0.5f};
    SetPosition(g_asset_editor.camera, current_center + world_offset);
}

static void UpdateView()
{
    PanView();
    ZoomView();

    // Frame
    if (WasButtonPressed(g_asset_editor.input, KEY_F) && g_asset_editor.selected_asset_count > 0)
        FrameView();

    // UI Scale controls
    if (IsButtonDown(g_asset_editor.input, KEY_LEFT_CTRL))
    {
        // Handle both = and + (shift + =) for increasing UI scale
        if (WasButtonPressed(g_asset_editor.input, KEY_EQUALS))
        {
            g_asset_editor.ui_scale = Min(g_asset_editor.ui_scale + 0.1f, 3.0f);
        }
        if (WasButtonPressed(g_asset_editor.input, KEY_MINUS))
        {
            g_asset_editor.ui_scale = Max(g_asset_editor.ui_scale - 0.1f, 0.3f);
        }
    }
}

void UpdateAssetEditor()
{
    g_asset_editor.input_locked = false;

    // Custom code for asset editor
    if (IsEditing())
    {
        EditableAsset& ea = *g_asset_editor.assets[g_asset_editor.edit_asset_index];
        switch (ea.type)
        {
        case EDITABLE_ASSET_TYPE_MESH:
            UpdateMeshEditor(ea);
            break;

        default:
            break;
        }
    }

    UpdateCamera();

    g_asset_editor.world_mouse_position = ScreenToWorld(g_asset_editor.camera, GetMousePosition());

    if (!IsEditing())
        g_asset_editor.hover_asset = HitTestAssets(g_asset_editor.world_mouse_position);

    // Enter / Exit edit mode
    if (!g_asset_editor.box_selecting && WasButtonPressed(g_asset_editor.input, KEY_TAB))
    {
        if (IsEditing())
        {
            g_asset_editor.edit_asset_index = -1;
            g_asset_editor.state = ASSET_EDITOR_STATE_NONE;
        }
        else if (g_asset_editor.selected_asset_count == 1)
        {
            g_asset_editor.edit_asset_index = GetFirstSelectedAsset();
            assert(g_asset_editor.edit_asset_index != -1);

            EditableAsset& ea = *g_asset_editor.assets[g_asset_editor.edit_asset_index];
            switch (ea.type)
            {
            case EDITABLE_ASSET_TYPE_MESH:
                g_asset_editor.state = ASSET_EDITOR_STATE_EDIT;
                InitMeshEditor(ea);
                break;

            default:
                break;
            }
        }
    }


    if (!IsEditing() && WasButtonPressed(g_asset_editor.input, MOUSE_LEFT))
    {
        // if (g_asset_editor.hover_asset != -1 && g_asset_editor.selected_asset == g_asset_editor.hover_asset )
        // {
        //     g_asset_editor.dragging = true;
        //     g_asset_editor.drag_start = GetMousePosition();
        //     g_asset_editor.drag_position_start = g_asset_editor.assets[g_asset_editor.selected_asset]->position;
        // }
        // else
        // {
        //     g_asset_editor.selected_asset = g_asset_editor.hover_asset;
        // }

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

    // if (!g_asset_editor.edit_mode && WasButtonReleased(g_asset_editor.input, MOUSE_LEFT) || WasButtonReleased(g_asset_editor.input, MOUSE_RIGHT))
    // {
    //     g_asset_editor.dragging = false;
    //     g_asset_editor.selected_vertex = -1;
    // }

    // if (WasButtonPressed(g_asset_editor.input, KEY_SPACE))
    // {
    //     int triangle = HitTestTriangle();
    //     if (triangle != -1)
    //     {
    //         SetTriangleColor(g_asset_editor.emesh, triangle, Vec2Int{g_asset_editor.emesh->triangles[triangle].color.x + 1, 1});
    //     }
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
    // if (g_asset_editor.dragging && g_asset_editor.selected_asset != -1)
    // {
    //     Vec2 drag_delta =
    //         g_asset_editor.world_mouse_position - ScreenToWorld(g_asset_editor.camera, g_asset_editor.drag_start);
    //
    //     Vec2 world = g_asset_editor.drag_position_start + drag_delta;
    //     if (IsButtonDown(g_asset_editor.input, KEY_LEFT_CTRL))
    //         world = SnapToGrid(world, true);
    //
    //     MoveTo(*g_asset_editor.assets[g_asset_editor.selected_asset], world);
    // }



    // Save
    if (!g_asset_editor.input_locked && WasButtonPressed(g_asset_editor.input, KEY_S) && IsButtonDown(g_asset_editor.input, KEY_LEFT_CTRL))
    {
        SaveEditableAssets();
    }

    UpdateBoxSelect();
    UpdateView();
    UpdateNotifications();
}

static void DrawBoxSelect()
{
    if (!g_asset_editor.box_selecting)
        return;

    Vec2 center = Vec2{
        (g_asset_editor.box_selection.min.x + g_asset_editor.box_selection.max.x) * 0.5f,
        (g_asset_editor.box_selection.min.y + g_asset_editor.box_selection.max.y) * 0.5f
    };
    Vec2 size = Vec2{
        g_asset_editor.box_selection.max.x - g_asset_editor.box_selection.min.x,
        g_asset_editor.box_selection.max.y - g_asset_editor.box_selection.min.y
    };

    // center
    BindColor(BOX_SELECT_COLOR);
    BindMaterial(g_asset_editor.vertex_material);
    BindTransform(TRS(center, 0, size * 0.5f));
    DrawMesh(g_asset_editor.edge_mesh);

    // outline
    float edge_width = g_asset_editor.zoom_ref_scale * BOX_SELECT_EDGE_WIDTH;
    BindColor(Color{0.2f, 0.6f, 1.0f, 0.8f});
    BindTransform(TRS(Vec2{center.x, g_asset_editor.box_selection.max.y}, 0, Vec2{size.x * 0.5f + edge_width, edge_width}));
    DrawMesh(g_asset_editor.edge_mesh);
    BindTransform(TRS(Vec2{center.x, g_asset_editor.box_selection.min.y}, 0, Vec2{size.x * 0.5f + edge_width, edge_width}));
    DrawMesh(g_asset_editor.edge_mesh);
    BindTransform(TRS(Vec2{g_asset_editor.box_selection.min.x, center.y}, 0, Vec2{edge_width, size.y * 0.5f + edge_width}));
    DrawMesh(g_asset_editor.edge_mesh);
    BindTransform(TRS(Vec2{g_asset_editor.box_selection.max.x, center.y}, 0, Vec2{edge_width, size.y * 0.5f + edge_width}));
    DrawMesh(g_asset_editor.edge_mesh);
}

void RenderView()
{
    BindCamera(g_asset_editor.camera);

    // Grid
    DrawGrid(g_asset_editor.camera);

    // Draw assets
    BindColor(COLOR_WHITE);
    BindMaterial(g_asset_editor.material);
    for (int i=0; i<g_asset_editor.asset_count; i++)
        DrawAsset(*g_asset_editor.assets[i]);

    // Draw edges
    if (IsEditing())
    {
        EditableAsset& ea = *g_asset_editor.assets[g_asset_editor.edit_asset_index];
        switch (ea.type)
        {
        case EDITABLE_ASSET_TYPE_MESH:
            RenderMeshEditor(ea);
            break;

        default:
            break;
        }
    }
    else
    {
        for (int i=0; i<g_asset_editor.asset_count; i++)
        {
            const EditableAsset& ea = *g_asset_editor.assets[i];
            if (!ea.selected)
                continue;

            DrawEdges(ea, 1, COLOR_SELECTED);
        }
    }

    DrawBoxSelect();
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
    InitApplication(&traits, argc, argv);

    g_asset_editor.camera = CreateCamera(ALLOCATOR_DEFAULT);
    g_asset_editor.material = CreateMaterial(ALLOCATOR_DEFAULT, g_assets.shaders._default);
    g_asset_editor.vertex_material = CreateMaterial(ALLOCATOR_DEFAULT, g_assets.shaders.ui);
    g_asset_editor.zoom = ZOOM_DEFAULT;
    g_asset_editor.ui_scale = 1.0f;
    g_asset_editor.dpi = 72.0f;
    g_asset_editor.selected_vertex = -1;
    g_asset_editor.edit_asset_index = -1;
    UpdateCamera();
    SetTexture(g_asset_editor.material, g_assets.textures.palette, 0);

    g_asset_editor.input = CreateInputSet(ALLOCATOR_DEFAULT);
    EnableButton(g_asset_editor.input, MOUSE_LEFT);
    EnableButton(g_asset_editor.input, MOUSE_RIGHT);
    EnableButton(g_asset_editor.input, MOUSE_MIDDLE);
    EnableButton(g_asset_editor.input, KEY_X);
    EnableButton(g_asset_editor.input, KEY_F);
    EnableButton(g_asset_editor.input, KEY_G);
    EnableButton(g_asset_editor.input, KEY_R);
    EnableButton(g_asset_editor.input, KEY_M);
    EnableButton(g_asset_editor.input, KEY_Q);
    EnableButton(g_asset_editor.input, KEY_A);
    EnableButton(g_asset_editor.input, KEY_V);
    EnableButton(g_asset_editor.input, KEY_ESCAPE);
    EnableButton(g_asset_editor.input, KEY_ENTER);
    EnableButton(g_asset_editor.input, KEY_SPACE);
    EnableButton(g_asset_editor.input, KEY_LEFT_CTRL);
    EnableButton(g_asset_editor.input, KEY_LEFT_SHIFT);
    EnableButton(g_asset_editor.input, KEY_TAB);
    EnableButton(g_asset_editor.input, KEY_S);
    EnableButton(g_asset_editor.input, KEY_EQUALS);
    EnableButton(g_asset_editor.input, KEY_MINUS);
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
    InitNotifications();

    g_asset_editor.asset_count = LoadEditableAssets(g_asset_editor.assets);
    g_asset_editor.state = ASSET_EDITOR_STATE_NONE;

    // todo: read path from editor config
    while (UpdateApplication())
    {
        BeginUI(UI_REF_WIDTH, UI_REF_HEIGHT);
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

