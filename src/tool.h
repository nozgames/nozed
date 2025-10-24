//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

// @box_select
extern void BeginBoxSelect(void (*callback)(const Bounds2& bounds));

// @move
extern void BeginMove(const Vec2& origin, void (*callback)(const Vec2& delta));extern void BeginMove(const Vec2& origin, void (*callback)(const Vec2& delta));