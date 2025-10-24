//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

// @box_select
extern void BeginBoxSelect(void (*callback)(const Bounds2& bounds));

// @move
struct MoveToolOptions {
    Vec2 origin;
    void (*update)(const Vec2& delta);
    void (*commit)(const Vec2& delta);
    void (*cancel)();
};

extern void BeginMove(const MoveToolOptions& options);
