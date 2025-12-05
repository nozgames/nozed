//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

struct MoveTool {
    Vec2 delta_position;
    MoveToolOptions options;
    Vec2 delta_scale;
};

static MoveTool g_move = {};

static void EndMove(bool commit) {
    if (commit && g_move.options.commit)
        g_move.options.commit(g_move.delta_position);
    else if (!commit && g_move.options.cancel)
        g_move.options.cancel();

    if (IsCommandInputActive())
        EndCommandInput();

    EndDrag();
    EndTool();
}

static void UpdateMove() {
    if (WasButtonPressed(MOUSE_LEFT)) {
        EndMove(true);
        return;
    }

    if (IsCommandInputActive() && WasButtonPressed(GetInputSet(), KEY_ESCAPE)) {
        g_move.delta_scale = VEC2_ONE;
        EndCommandInput();
        return;
    }

    if (!g_view.drag || WasButtonPressed(GetInputSet(), KEY_ESCAPE)) {
        EndMove(false);
        return;
    }

    if (WasButtonPressed(GetInputSet(), KEY_X)) {
        static CommandHandler commands[] = {{ nullptr, nullptr, nullptr }};
        BeginCommandInput({.commands=commands, .prefix="x", .input=GetInputSet()});
        g_move.delta_scale = {1.0f, 0.0f};
    }

    if (WasButtonPressed(GetInputSet(), KEY_Y)) {
        static CommandHandler commands[] = {{ nullptr, nullptr, nullptr }};
        BeginCommandInput({.commands=commands, .prefix="y", .input=GetInputSet()});
        g_move.delta_scale = {0.0f, 1.0f};
    }

    Vec2 delta = g_view.drag_world_delta * g_move.delta_scale;
    if (g_move.delta_position == delta)
        return;

    g_move.delta_position = delta;

    if (g_move.options.update)
        g_move.options.update(delta);
}

void BeginMoveTool(const MoveToolOptions& options) {
    static ToolVtable vtable = {
        .update = UpdateMove
    };

    BeginTool({
        .type = TOOL_TYPE_MOVE,
        .vtable = vtable,
        .input = g_view.input_tool
    });

    g_move = {};
    g_move.options = options;
    g_move.delta_scale = VEC2_ONE;

    BeginDrag();
}
