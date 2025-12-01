//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

constexpr float ROTATE_TOOL_CENTER_SIZE = 0.2f;
constexpr float WEIGHT_TOOL_SIZE = 2.0f;
constexpr float CIRCLE_CONTROL_OUTLINE_SIZE = 0.13f;
constexpr float CIRCLE_CONTROL_SIZE = 0.12f;

struct WeightTool {
    WeightToolOptions options;
    float initial_weights[MAX_VERTICES];
};

static WeightTool g_weight = {};

static void EndWeightTool(bool commit) {
    if (commit && g_weight.options.commit)
        g_weight.options.commit();
    else if (!commit && g_weight.options.cancel)
        g_weight.options.cancel();

    EndDrag();
    EndTool();
}

static void UpdateVertexWeightTool() {
    if (WasButtonPressed(GetInputSet(), MOUSE_LEFT)) {
        EndWeightTool(true);
        return;
    }

    if (!g_view.drag || WasButtonPressed(GetInputSet(), KEY_ESCAPE)) {
        EndWeightTool(false);
        return;
    }

    float delta = (g_view.mouse_position.y - g_view.drag_position.y) / (g_view.dpi * WEIGHT_TOOL_SIZE);

    for (int i=0; i<g_weight.options.vertex_count; i++) {
        WeightToolVertex& v = g_weight.options.vertices[i];
        v.weight = Clamp(g_weight.initial_weights[i] - delta, g_weight.options.min_weight, g_weight.options.max_weight);
        if (g_weight.options.update_vertex)
            g_weight.options.update_vertex(v.weight, v.user_data);
    }

    if (g_weight.options.update)
        g_weight.options.update();
}

static void DrawVertexWeightTool() {
    for (int i=0; i<g_weight.options.vertex_count; i++) {
        WeightToolVertex& v = g_weight.options.vertices[i];
        BindColor(COLOR_VERTEX_SELECTED);
        DrawMesh(g_view.circle_mesh, TRS(v.position, 0, VEC2_ONE * CIRCLE_CONTROL_OUTLINE_SIZE * g_view.zoom_ref_scale));
    }

    for (int i=0; i<g_weight.options.vertex_count; i++) {
        WeightToolVertex& v = g_weight.options.vertices[i];
        f32 h = (v.weight - g_weight.options.min_weight) / (g_weight.options.max_weight - g_weight.options.min_weight);
        i32 arc = Clamp((i32)(100 * h), 0, 100);

        BindColor(COLOR_BLACK);
        DrawMesh(g_view.circle_mesh, TRS(v.position, 0, VEC2_ONE * CIRCLE_CONTROL_SIZE * g_view.zoom_ref_scale));
        BindColor(COLOR_VERTEX_SELECTED);
        DrawMesh(g_view.arc_mesh[arc], TRS(v.position, 0, VEC2_ONE * CIRCLE_CONTROL_SIZE * g_view.zoom_ref_scale));
    }

}

void BeginWeightTool(const WeightToolOptions& options) {
    static ToolVtable vtable = {
        .update = UpdateVertexWeightTool,
        .draw = DrawVertexWeightTool,
    };

    BeginTool({
        .type = TOOL_TYPE_WEIGHT,
        .vtable = vtable,
        .input = g_view.input_tool
    });

    g_weight = {};
    g_weight.options = options;

    for (int i=0; i<options.vertex_count; i++)
        g_weight.initial_weights[i] = options.vertices[i].weight;

    BeginDrag();
}
