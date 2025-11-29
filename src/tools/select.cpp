//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

struct KnifeTool {
    SelectToolOptions options;
};

static KnifeTool g_knife_tool = {};

static void EndSelectTool(bool commit) {
    if (!commit && g_knife_tool.options.cancel)
        g_knife_tool.options.cancel();
    else if (commit && g_knife_tool.options.commit)
        g_knife_tool.options.commit(g_view.mouse_world_position);

    EndTool();
}

static void UpdateSelectTool() {
    if (g_knife_tool.options.update)
        g_knife_tool.options.update(g_view.mouse_world_position);

    if (WasButtonPressed(KEY_ESCAPE)) {
        EndSelectTool(false);
        return;
    }

    if (WasButtonPressed(MOUSE_LEFT)) {
        EndSelectTool(true);
        return;
    }
}

void BeginSelectTool(const SelectToolOptions& options) {
    static ToolVtable vtable = {
        .update = UpdateSelectTool
    };

    BeginTool({
        .type = TOOL_TYPE_SELECT,
        .vtable = vtable,
        .input = g_view.input_tool
    });

    g_knife_tool.options = options;

    SetCursor(SYSTEM_CURSOR_SELECT);
}
