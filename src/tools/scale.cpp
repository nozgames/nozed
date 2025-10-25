//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

constexpr float SCALE_TOOL_CENTER_SIZE = 0.2f;
constexpr float SCALE_TOOL_WIDTH = 0.02f;

struct ScaleTool {
    float last_scale;
    ScaleToolOptions options;
};

static ScaleTool g_scale = {};

static void EndMove(bool commit) {
    if (commit && g_scale.options.commit)
        g_scale.options.commit(g_scale.last_scale);
    else if (!commit && g_scale.options.cancel)
        g_scale.options.cancel();

    EndDrag();
    EndTool();
}

static void UpdateScale() {
    if (WasButtonPressed(GetInputSet(), MOUSE_LEFT)) {
        EndMove(true);
        return;
    }

    if (!g_view.drag || WasButtonPressed(GetInputSet(), KEY_ESCAPE)) {
        EndMove(false);
        return;
    }

    float scale =
        Length(g_view.mouse_world_position - g_scale.options.origin) -
        Length(g_view.drag_world_position - g_scale.options.origin);

    if (g_scale.last_scale == scale)
        return;

    g_scale.last_scale = scale;

    if (g_scale.options.update)
        g_scale.options.update(1.0f + scale);
}

static void DrawScale() {
    BindColor(SetAlpha(COLOR_CENTER, 0.75f));
    DrawVertex(g_scale.options.origin, SCALE_TOOL_CENTER_SIZE * 0.75f);
    BindColor(COLOR_CENTER);
    DrawLine(g_view.mouse_world_position, g_scale.options.origin, SCALE_TOOL_WIDTH);
    BindColor(COLOR_ORIGIN);
    DrawVertex(g_view.mouse_world_position, SCALE_TOOL_CENTER_SIZE);
}

void BeginScaleTool(const ScaleToolOptions& options) {
    static ToolVtable vtable = {
        .update = UpdateScale,
        .draw = DrawScale,
    };

    BeginTool({
        .type = TOOL_TYPE_SCALE,
        .vtable = vtable,
        .input = g_view.input_tool
    });


    g_scale.options = options;

    BeginDrag();
}
