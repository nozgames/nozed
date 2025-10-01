//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#include <editor.h>

constexpr float DEFAULT_LINE_WIDTH = 0.01f;
constexpr float DEFAULT_VERTEX_SIZE = 0.1f;
constexpr float DEFAULT_DASH_LENGTH = 0.1f;
constexpr float DEFAULT_ARROW_SIZE = 0.3f;
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
    BindTransform(TRS(mid, dir, Vec2{length * 0.5f, width * g_view.zoom_ref_scale}));
    DrawMesh(g_view.edge_mesh);
}

void DrawDashedLine(const Vec2& v0, const Vec2& v1, f32 width, f32 length)
{
    Vec2 line_dir = Normalize(v1 - v0);
    float line_len = Length(v1 - v0);

    length *= g_view.zoom_ref_scale;
    float scale = length * 0.5f;

    int count = 0;
    for (float pos = length/2; pos < line_len && count < 100; pos += length * 2, count++)
    {
        BindTransform(TRS(v0 + line_dir * pos, line_dir, Vec2{scale, width * g_view.zoom_ref_scale}));
        DrawMesh(g_view.edge_mesh);
    }
}

void DrawDashedLine(const Vec2& v0, const Vec2& v1)
{
    DrawDashedLine(v0, v1, DEFAULT_LINE_WIDTH, DEFAULT_DASH_LENGTH);
}
void DrawVertex(const Vec2& v)
{
    DrawVertex(v, DEFAULT_VERTEX_SIZE);
}

void DrawVertex(const Vec2& v, f32 size)
{
    BindTransform(TRS(v, 0, VEC2_ONE * g_view.zoom_ref_scale * size));
    DrawMesh(g_view.vertex_mesh);
}

void DrawArrow(const Vec2& v, const Vec2& dir, f32 size)
{
    BindTransform(TRS(v, dir, VEC2_ONE * g_view.zoom_ref_scale * size));
    DrawMesh(g_view.arrow_mesh);
}

void DrawArrow(const Vec2& v, const Vec2& dir)
{
    DrawArrow(v, dir, DEFAULT_ARROW_SIZE);
}

void DrawOrigin(EditorAsset* ea)
{
    BindMaterial(g_view.vertex_material);
    BindColor(COLOR_ORIGIN_BORDER);
    DrawVertex(ea->position, ORIGIN_BORDER_SIZE);
    BindColor(COLOR_ORIGIN);
    DrawVertex(ea->position, ORIGIN_SIZE);
}

void DrawBounds(EditorAsset* ea, float expand)
{
    BindMaterial(g_view.vertex_material);
    BindColor(COLOR_BLACK);
    Bounds2 b = Expand(GetBounds(ea), expand);
    Vec2 center = GetCenter(b) + ea->position;
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

void DrawBone(const Mat3& transform, const Mat3& parent_transform, const Vec2& position)
{
    Vec2 p0 = TransformPoint(transform);
    Vec2 p1 = TransformPoint(transform, Vec2 {1, 0});
    Vec2 pp = TransformPoint(parent_transform);
    DrawDashedLine(pp + position, p0 + position);
    DrawVertex(p0 + position);
    DrawVertex(p1 + position);
    DrawBone(p0 + position, p1 + position);
}
