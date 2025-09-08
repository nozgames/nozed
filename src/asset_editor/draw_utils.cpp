//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"

void DrawLine(const Vec2& v0, const Vec2& v1, f32 width)
{
    Vec2 mid = (v0 + v1) * 0.5f;
    Vec2 dir = Normalize(v1 - v0);
    float length = Length(v1 - v0);
    BindTransform(TRS(mid, dir, Vec2{length * 0.5f, width * g_asset_editor.zoom_ref_scale}));
    DrawMesh(g_asset_editor.edge_mesh);
}

void DrawVertex(const Vec2& v, f32 size)
{
    BindTransform(TRS(v, 0, VEC2_ONE * g_asset_editor.zoom_ref_scale * size));
    DrawMesh(g_asset_editor.vertex_mesh);
}
