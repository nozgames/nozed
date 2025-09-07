//
//  MeshZ - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"

constexpr float GRID_SPACING = 1.0f;
constexpr float SECONDARY_GRID_FADE_MIN = 0.02f;  // Start fading in when grid cell is 2% of screen
constexpr float SECONDARY_GRID_FADE_MAX = 0.1f;  // Fully visible when grid cell is 10% of screen
constexpr Color PRIMARY_GRID_COLOR = { 0.3f, 0.3f, 0.3f, 0.8f };
constexpr Color SECONDARY_GRID_COLOR = { 0.2f, 0.2f, 0.2f, 0.6f };

struct Grid
{
    Material* material;
    Mesh* quad_mesh;
    float grid_spacing;
};

static Grid g_grid = {};

static void DrawGridLines(Camera* camera, float zoom, float spacing, const Color& color, float alpha)
{
    if (alpha <= 0.0f) return;
    
    // Get camera bounds to determine which lines to draw
    Bounds2 bounds = GetBounds(camera);
    float left = bounds.min.x;
    float right = bounds.max.x;
    float bottom = bounds.min.y;
    float top = bounds.max.y;
    
    // Calculate line thickness based on zoom
    float line_thickness = 0.005f * zoom / 10.0f;
    
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

void DrawGrid(Camera* camera, float zoom)
{
    BindMaterial(g_grid.material);
    
    // Get screen dimensions to calculate grid cell size as percentage of screen
    Bounds2 bounds = GetBounds(camera);
    float screen_width = bounds.max.x - bounds.min.x;
    float screen_height = bounds.max.y - bounds.min.y;
    float screen_size = Min(screen_width, screen_height);
    
    // Calculate what grid level we should be at
    float base_cell_size = g_grid.grid_spacing / screen_size;
    
    // Find the integer grid level (how many times we've scaled by 10)
    float log_scale = log10f(base_cell_size / SECONDARY_GRID_FADE_MAX);
    int grid_level = (int)floorf(log_scale);
    float fractional_part = log_scale - grid_level;
    
    // Calculate primary grid spacing at this level
    float primary_spacing = g_grid.grid_spacing * powf(0.1f, grid_level);
    
    // Draw primary grid (always visible)
    DrawGridLines(camera, zoom, primary_spacing, PRIMARY_GRID_COLOR, 1.0f);
    
    // Calculate secondary grid (10x smaller)
    float secondary_spacing = primary_spacing * 0.1f;
    
    // Use fractional part to determine secondary grid alpha
    // When fractional_part is 0, we just switched to this level (secondary invisible)
    // When fractional_part is 1, we're about to switch to next level (secondary fully visible)
    float fade_alpha = fractional_part;
    
    if (fade_alpha > 0.0f)
    {
        DrawGridLines(camera, zoom, secondary_spacing, SECONDARY_GRID_COLOR, fade_alpha);
    }
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

void ShutdownGrid()
{
    g_grid = {};
}
