//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

struct MoveTool {
    Vec2 last_delta;
    MoveToolOptions options;
};

static MoveTool g_move = {};

static void EndMove(bool commit) {
    if (commit && g_move.options.commit)
        g_move.options.commit(g_move.last_delta);
    else if (!commit && g_move.options.cancel)
        g_move.options.cancel();

    EndDrag();
    EndTool();
}

static void UpdateMove() {
    if (WasButtonPressed(GetInputSet(), MOUSE_LEFT)) {
        EndMove(true);
        return;
    }

    if (!g_view.drag || WasButtonPressed(GetInputSet(), KEY_ESCAPE)) {
        EndMove(false);
        return;
    }

    Vec2 delta = IsCtrlDown(GetInputSet())
        ? SnapToGrid(g_view.mouse_world_position) - SnapToGrid(g_view.drag_world_position)
        : g_view.drag_world_delta;

    if (g_move.last_delta == delta)
        return;

    g_move.last_delta = delta;

    if (g_move.options.update)
        g_move.options.update(delta);
}

void BeginMove(const MoveToolOptions& options) {
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

    BeginDrag();
}
