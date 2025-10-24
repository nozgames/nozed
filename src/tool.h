//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

// @box_select
extern void BeginBoxSelect(void (*callback)(const Bounds2& bounds));

// @move
struct MoveToolOptions {
    void (*update)(const Vec2& delta);
    void (*commit)(const Vec2& delta);
    void (*cancel)();
};

extern void BeginMove(const MoveToolOptions& options);

// @scale
struct ScaleToolOptions {
    Vec2 origin;
    void (*update)(float scale);
    void (*commit)(float scale);
    void (*cancel)();
};

extern void BeginScale(const ScaleToolOptions& options);

// @rotate
struct RotateToolOptions {
    Vec2 origin;
    void (*update)(float angle);
    void (*commit)(float angle);
    void (*cancel)();
};

extern void BeginRotate(const RotateToolOptions& options);
