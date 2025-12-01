//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

struct SelectTool {
    SelectToolOptions options;
};

static SelectTool g_select_tool = {};

static void EndSelectTool(bool commit) {
    if (!commit && g_select_tool.options.cancel)
        g_select_tool.options.cancel();
    else if (commit && g_select_tool.options.commit)
        g_select_tool.options.commit(g_view.mouse_world_position);

    EndTool();
}

static void UpdateSelectTool() {
    if (g_select_tool.options.update)
        g_select_tool.options.update(g_view.mouse_world_position);

    if (WasButtonPressed(KEY_ESCAPE)) {
        EndSelectTool(false);
        return;
    }

    if (WasButtonPressed(MOUSE_LEFT)) {
        EndSelectTool(true);
        return;
    }
}

static void DrawSelectTool() {
    if (g_select_tool.options.draw)
        g_select_tool.options.draw(g_view.mouse_world_position);
}

void BeginSelectTool(const SelectToolOptions& options) {
    static ToolVtable vtable = {
        .update = UpdateSelectTool,
        .draw = DrawSelectTool
    };

    BeginTool({
        .type = TOOL_TYPE_SELECT,
        .vtable = vtable,
        .input = g_view.input_tool
    });

    g_select_tool.options = options;

    SetCursor(SYSTEM_CURSOR_SELECT);
}
