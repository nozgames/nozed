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

extern void BeginMoveTool(const MoveToolOptions& options);

// @scale
struct ScaleToolOptions {
    Vec2 origin;
    void (*update)(float scale);
    void (*commit)(float scale);
    void (*cancel)();
};

extern void BeginScaleTool(const ScaleToolOptions& options);
extern void SetScaleToolOrigin(const Vec2& origin);

// @rotate
struct RotateToolOptions {
    Vec2 origin;
    void (*update)(float angle);
    void (*commit)(float angle);
    void (*cancel)();
};

extern void BeginRotateTool(const RotateToolOptions& options);

// @weight
struct WeightToolVertex {
    Vec2 position;
    float weight;
    void* user_data;
};

struct WeightToolOptions {
    int vertex_count;
    WeightToolVertex vertices[MAX_VERTICES];
    float min_weight;
    float max_weight;

    void (*update)();
    void (*commit)();
    void (*cancel)();
    void (*update_vertex)(float weight, void* user_data);
};

extern void BeginWeightTool(const WeightToolOptions& options);

// @select
struct SelectToolOptions {
    void (*update)(const Vec2& position);
    void (*commit)(const Vec2& position);
    void (*cancel)();
};

extern void BeginSelectTool(const SelectToolOptions& options);


// @knife

extern void BeginKnifeTool(MeshData* mesh);