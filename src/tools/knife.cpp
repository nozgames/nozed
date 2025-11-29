//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

struct KnifePoint {
    Vec2 position;
    int vertex_index;
    int face_index;
    int edge_index;
};

struct KnifeCut {
    Vec2 position;
    int vertex_index;
    int face_index;
    int edge_index;
};

struct KnifeTool {
    KnifeCut cuts[256];
    int cut_count = 0;
    MeshData* mesh;
    Vec2 vertices[MAX_VERTICES];
    int vertex_count;
};

static KnifeTool g_knife_tool = {};

static void EndKnifeTool(bool commit) {
    (void)commit;
    g_knife_tool.mesh = nullptr;
    EndTool();
}

static void DrawKnifeTool() {
    BindColor(COLOR_VERTEX_SELECTED);
    for (int i=0; i<g_knife_tool.cut_count-1; i++) {
        DrawDashedLine(
            g_knife_tool.cuts[i].position + g_knife_tool.mesh->position,
            g_knife_tool.cuts[i+1].position + g_knife_tool.mesh->position);
    }

    for (int i=0; i<g_knife_tool.cut_count; i++) {
        DrawVertex(g_knife_tool.cuts[i].position + g_knife_tool.mesh->position);
    }

    BindColor(COLOR_GREEN);
    for (int i=0; i<g_knife_tool.vertex_count; i++) {
        DrawVertex(g_knife_tool.vertices[i] + g_knife_tool.mesh->position);
    }
}

static void UpdateKnifeTool() {
    // if (g_select_tool.options.update)
    //     g_select_tool.options.update(g_view.mouse_world_position);

    if (WasButtonPressed(KEY_ESCAPE)) {
        EndKnifeTool(false);
        return;
    }

    if (WasButtonPressed(KEY_ENTER)) {
        EndKnifeTool(false);
        return;
    }

    if (WasButtonPressed(MOUSE_LEFT)) {
        int vertex_index = HitTestVertex(g_knife_tool.mesh, g_view.mouse_world_position);
        int edge_index = vertex_index == -1
            ? HitTestEdge(g_knife_tool.mesh, g_view.mouse_world_position, nullptr)
            : -1;
        int face_index = (vertex_index == -1 && edge_index == -1)
            ? HitTestFace(g_knife_tool.mesh, Translate(g_knife_tool.mesh->position), g_view.mouse_world_position)
            : -1;

        Vec2 position = g_view.mouse_world_position - g_knife_tool.mesh->position;
        if (vertex_index != -1)
            position = g_knife_tool.mesh->vertices[vertex_index].position;

        g_knife_tool.cuts[g_knife_tool.cut_count++] = {
            .position = position,
            .vertex_index = vertex_index,
            .face_index = face_index,
            .edge_index = edge_index,
        };

        // if the new cut crosses any edges, create vertices at the intersections
        if (g_knife_tool.cut_count <= 1)
            return;

        for (int i=0; i<g_knife_tool.mesh->edge_count; i++) {
            EdgeData& e = g_knife_tool.mesh->edges[i];
            Vec2 v0 = g_knife_tool.mesh->vertices[e.v0].position;
            Vec2 v1 = g_knife_tool.mesh->vertices[e.v1].position;
            Vec2 intersection;
            if (!OverlapLine(
                g_knife_tool.cuts[g_knife_tool.cut_count-2].position,
                g_knife_tool.cuts[g_knife_tool.cut_count-1].position,
                v0,
                v1,
                &intersection))
                continue;

            g_knife_tool.vertices[g_knife_tool.vertex_count++] = intersection;
        }

        return;
    }
}

void BeginKnifeTool(MeshData* mesh) {
    static ToolVtable vtable = {
        .update = UpdateKnifeTool,
        .draw = DrawKnifeTool
    };

    BeginTool({
        .type = TOOL_TYPE_SELECT,
        .vtable = vtable,
        .input = g_view.input_tool,
        .hide_selected = true
    });

    g_knife_tool.mesh = mesh;
    g_knife_tool.cut_count = 0;
    g_knife_tool.vertex_count = 0;

    SetCursor(SYSTEM_CURSOR_SELECT);
}
