//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

constexpr float SCALE_TOOL_CENTER_SIZE = 0.2f;
constexpr float SCALE_TOOL_WIDTH = 0.02f;

struct ScaleTool {
    Vec2 last_scale;
    bool last_ctrl;
    Vec2 delta_scale;
    ScaleToolOptions options;
};

static ScaleTool g_scale = {};

static void EndScale(bool commit) {
    if (commit && g_scale.options.commit)
        g_scale.options.commit(g_scale.last_scale);
    else if (!commit && g_scale.options.cancel)
        g_scale.options.cancel();

    if (IsCommandInputActive())
        EndCommandInput();

    EndDrag();
    EndTool();
}

static void UpdateScale() {
    if (WasButtonPressed(MOUSE_LEFT)) {
        EndScale(true);
        return;
    }

    if (IsCommandInputActive() && WasButtonPressed(GetInputSet(), KEY_ESCAPE)) {
        g_scale.delta_scale = VEC2_ONE;
        EndCommandInput();
        return;
    }

    if (!g_view.drag || WasButtonPressed(GetInputSet(), KEY_ESCAPE)) {
        EndScale(false);
        return;
    }

    if (WasButtonPressed(GetInputSet(), KEY_X)) {
        static CommandHandler commands[] = {{ nullptr, nullptr, nullptr }};
        BeginCommandInput({.commands=commands, .prefix="x", .input=GetInputSet()});
        g_scale.delta_scale = {1.0f, 0.0f};
    }

    if (WasButtonPressed(GetInputSet(), KEY_Y)) {
        static CommandHandler commands[] = {{ nullptr, nullptr, nullptr }};
        BeginCommandInput({.commands=commands, .prefix="y", .input=GetInputSet()});
        g_scale.delta_scale = {0.0f, 1.0f};
    }

    Vec2 delta_scale = g_scale.delta_scale *
        (Length(g_view.mouse_world_position - g_scale.options.origin) -
         Length(g_view.drag_world_position - g_scale.options.origin));

    bool ctrl = IsCtrlDown();
    if (g_scale.last_scale == delta_scale && ctrl == g_scale.last_ctrl)
        return;

    g_scale.last_scale = delta_scale;
    g_scale.last_ctrl = ctrl;

    if (g_scale.options.update)
        g_scale.options.update(VEC2_ONE + delta_scale);
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
    g_scale.delta_scale = VEC2_ONE;

    BeginDrag();
}

void SetScaleToolOrigin(const Vec2& origin) {
    g_scale.options.origin = origin;
}