//
//  MeshZ - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"

constexpr float GRID_SPACING = 1.0f;
constexpr float MIN_GRID_PIXELS = 50.0f;
constexpr float MAX_GRID_PIXELS = 500.0f;
constexpr float SECONDARY_GRID_FADE_MIN = 0.02f;  // Start fading in when grid cell is 2% of screen
constexpr float SECONDARY_GRID_FADE_MAX = 0.1f;  // Fully visible when grid cell is 10% of screen
constexpr float TRANSITION_START = MAX_GRID_PIXELS * 0.3f; // Start fading in secondary at 30% of max
constexpr float TRANSITION_END = MIN_GRID_PIXELS; // Complete transition at min
constexpr Color PRIMARY_GRID_COLOR = { 0.0f, 0.0f, 0.0f, 0.3f };
constexpr Color SECONDARY_GRID_COLOR = { 0.0f, 0.0f, 0.0f, 0.1f };

struct Grid
{
    Material* material;
    Mesh* quad_mesh;
    float grid_spacing;
};

static Grid g_grid = {};

static void DrawGridLines(Camera* camera, float spacing, const Color& color, float alpha)
{
    if (alpha <= 0.0f) return;
    
    // Get camera bounds to determine which lines to draw
    Bounds2 bounds = GetBounds(camera);
    float left = bounds.min.x;
    float right = bounds.max.x;
    float bottom = bounds.min.y;
    float top = bounds.max.y;
    
    // Calculate line thickness based on world-to-screen scale
    // Use world units for consistent visual thickness
    Vec2Int screen_size = GetScreenSize();
    float world_height = top - bottom;
    float pixels_per_world_unit = screen_size.y / world_height;
    float line_thickness = 1.0f / pixels_per_world_unit; // 1 pixel thick
    
    Color line_color = color;
    line_color.a *= alpha;
    BindColor(line_color);
    
    // Draw vertical lines
    float start_x = floorf(left / spacing) * spacing;
    for (float x = start_x; x <= right + spacing; x += spacing)
    {
        Vec2 line_center = { x, (top + bottom) * 0.5f };
        Vec2 line_scale = { line_thickness, (top - bottom) * 0.5f };
        BindTransform(TRS(line_center, 0, line_scale));
        DrawMesh(g_grid.quad_mesh);
    }
    
    // Draw horizontal lines
    float start_y = floorf(bottom / spacing) * spacing;
    for (float y = start_y; y <= top + spacing; y += spacing)
    {
        Vec2 line_center = { (left + right) * 0.5f, y };
        Vec2 line_scale = { (right - left) * 0.5f, line_thickness };
        BindTransform(TRS(line_center, 0, line_scale));
        DrawMesh(g_grid.quad_mesh);
    }
}

static void DrawGridInternal(Camera* camera, float min_pixels, float grid_spacing, float min_alpha, float max_alpha)
{
    BindMaterial(g_grid.material);

    // Use camera to find how big this spacing is on screen
    Vec2 world_0 = WorldToScreen(camera, Vec2{0, 0});
    Vec2 world_1 = WorldToScreen(camera, Vec2{1.0f, 0});
    f32 pixels_per_grid = Length(world_1 - world_0);

    // Scale up by 10x as long as grid is smaller than threshold
    while (pixels_per_grid < min_pixels)
    {
        grid_spacing *= 10.0f;
        pixels_per_grid *= 10.0f;
    }

    // Scale down by 10x as long as grid is larger than threshold * 10
    while (pixels_per_grid > min_pixels * 10.0f)
    {
        grid_spacing *= 0.1f;
        pixels_per_grid *= 0.1f;
    }

    f32 alpha = Mix(min_alpha, max_alpha, (pixels_per_grid - min_pixels) / (min_pixels * 10.0f));
    DrawGridLines(camera, grid_spacing, PRIMARY_GRID_COLOR, alpha);
}

void DrawGrid(Camera* camera)
{
    BindMaterial(g_grid.material);
    DrawGridInternal(camera, 72.0f, 1.0f, 1, 1);
    DrawGridInternal(camera, 72.0f, 0.1f, 0, 1);
}

void InitGrid(Allocator* allocator)
{
    g_grid.material = CreateMaterial(allocator, g_assets.shaders.ui);
    g_grid.grid_spacing = GRID_SPACING;

    // Create a simple quad mesh for grid lines
    MeshBuilder* builder = CreateMeshBuilder(allocator, 4, 6);
    AddVertex(builder, { -1, -1});
    AddVertex(builder, {  1, -1});
    AddVertex(builder, {  1,  1});
    AddVertex(builder, { -1,  1});
    AddTriangle(builder, 0, 1, 2);
    AddTriangle(builder, 2, 3, 0);
    g_grid.quad_mesh = CreateMesh(allocator, builder, NAME_NONE);

    Free(builder);
}

Vec2 SnapToGrid(const Vec2& position, bool secondary)
{
    float spacing = secondary ? (g_grid.grid_spacing * 0.1f) : g_grid.grid_spacing;
    
    return Vec2{
        roundf(position.x / spacing) * spacing,
        roundf(position.y / spacing) * spacing
    };
}

void ShutdownGrid()
{
    g_grid = {};
}
