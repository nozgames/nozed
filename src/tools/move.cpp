//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

struct MoveTool {
    Vec2 origin;
    Vec2 last_delta;
    void (*callback)(const Vec2& delta);
};

static MoveTool g_move = {};

static void EndMove() {
    EndDrag();
    EndTool();
}

static void UpdateMove() {
    if (!g_view.drag) {
        EndMove();
        return;
    }

    if (WasButtonPressed(GetInputSet(), MOUSE_LEFT)) {
        EndMove();
        return;
    }

    if (WasButtonPressed(GetInputSet(), KEY_ESCAPE)) {
        EndMove();
        return;
    }

    Vec2 delta = IsCtrlDown(GetInputSet())
        ? SnapToGrid(g_view.mouse_world_position) - SnapToGrid(g_view.drag_world_position)
        : g_view.drag_world_delta;

    if (g_move.last_delta == delta)
        return;

    g_move.last_delta = delta;
    g_move.callback(delta);
}

static void DrawMove() {

}

void BeginMove(const Vec2& origin, void (*callback)(const Vec2& delta)) {
    assert(callback);

    static ToolVtable vtable = {
        .update = UpdateMove,
        .draw = DrawMove,
    };

    BeginTool({
        .type = TOOL_TYPE_MOVE,
        .vtable = vtable,
        .input = g_view.input_tool
    });

    g_move = {};
    g_move.callback = callback;
    g_move.origin = origin;

    BeginDrag();
}
