//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#include "../asset_editor/asset_editor.h"

constexpr float DEFAULT_LINE_WIDTH = 0.01f;
constexpr float DEFAULT_VERTEX_SIZE = 0.08f;
constexpr float ORIGIN_SIZE = 0.1f;
constexpr float ORIGIN_BORDER_SIZE = 0.12f;
constexpr float BONE_WIDTH = 0.10f;

void DrawLine(const Vec2& v0, const Vec2& v1)
{
    DrawLine(v0, v1, DEFAULT_LINE_WIDTH);
}

void DrawLine(const Vec2& v0, const Vec2& v1, f32 width)
{
    Vec2 mid = (v0 + v1) * 0.5f;
    Vec2 dir = Normalize(v1 - v0);
    float length = Length(v1 - v0);
    BindTransform(TRS(mid, dir, Vec2{length * 0.5f, width * g_asset_editor.zoom_ref_scale}));
    DrawMesh(g_asset_editor.edge_mesh);
}

void DrawVertex(const Vec2& v)
{
    DrawVertex(v, DEFAULT_VERTEX_SIZE);
}

void DrawVertex(const Vec2& v, f32 size)
{
    BindTransform(TRS(v, 0, VEC2_ONE * g_asset_editor.zoom_ref_scale * size));
    DrawMesh(g_asset_editor.vertex_mesh);
}

void DrawOrigin(const Vec2& position)
{
    BindMaterial(g_asset_editor.vertex_material);
    BindColor(COLOR_ORIGIN_BORDER);
    DrawVertex(position, ORIGIN_BORDER_SIZE);
    BindColor(COLOR_ORIGIN);
    DrawVertex(position, ORIGIN_SIZE);
}

void DrawOrigin(const EditorAsset& ea)
{
    DrawOrigin(ea.position);
}

void DrawBounds(const EditorAsset& ea, float expand)
{
    BindMaterial(g_asset_editor.vertex_material);
    BindColor(COLOR_BLACK);
    Bounds2 b = Expand(GetBounds(ea), expand);
    Vec2 center = GetCenter(b) + ea.position;
    Vec2 size = GetSize(b);
    DrawLine ({center.x - size.x * 0.5f, center.y - size.y * 0.5f}, {center.x + size.x * 0.5f, center.y - size.y * 0.5f});
    DrawLine ({center.x + size.x * 0.5f, center.y - size.y * 0.5f}, {center.x + size.x * 0.5f, center.y + size.y * 0.5f});
    DrawLine ({center.x + size.x * 0.5f, center.y + size.y * 0.5f}, {center.x - size.x * 0.5f, center.y + size.y * 0.5f});
    DrawLine ({center.x - size.x * 0.5f, center.y + size.y * 0.5f}, {center.x - size.x * 0.5f, center.y - size.y * 0.5f});
}

void DrawBone(const Vec2& a, const Vec2& b)
{
    float l = Length(b - a);
    float s = l * BONE_WIDTH;
    Vec2 d = Normalize(b - a);
    Vec2 c = a + d * s;
    Vec2 n = Perpendicular(d);
    Vec2 aa = c + n * s;
    Vec2 bb = c - n * s;

    DrawLine(a, bb);
    DrawLine(a, aa);
    DrawLine(aa, b);
    DrawLine(bb, b);
}
