//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

constexpr float VERTEX_WEIGHT_OUTLINE_SIZE = 0.13f;
constexpr float VERTEX_WEIGHT_CONTROL_SIZE = 0.12f;
constexpr float WEIGHT_TOOL_SIZE = 2.0f;

struct VertexWeightTool {
    VertexWeightToolOptions options;
    float initial_weights[MAX_VERTICES * MESH_MAX_VERTEX_WEIGHTS];
};

static VertexWeightTool g_vertex_weight = {};

static void EndVertexWeightTool(bool commit) {
    MeshData* m = g_vertex_weight.options.mesh;
    if (commit) {
        MarkModified(m);
        MarkDirty(m);
    } else {

        int vertex_count = g_vertex_weight.options.vertex_count;
        for (int vertex_index=0; vertex_index<vertex_count; vertex_index++)
            for (int weight_index=0; weight_index<MESH_MAX_VERTEX_WEIGHTS; weight_index++)
                m->vertices[g_vertex_weight.options.vertices[vertex_index]].weights[weight_index].weight =
                    g_vertex_weight.initial_weights[vertex_index * MESH_MAX_VERTEX_WEIGHTS + weight_index];

        CancelUndo();
    }

    EndDrag();
    EndTool();
}

static void UpdateVertexWeightTool() {
    if (!g_view.drag) {
        EndVertexWeightTool(true);
        return;
    }

    if (WasButtonPressed(GetInputSet(), KEY_ESCAPE)) {
        EndVertexWeightTool(false);
        return;
    }

    float delta = (g_view.mouse_position.y - g_view.drag_position.y) / (g_view.dpi * WEIGHT_TOOL_SIZE);
    MeshData* m = g_vertex_weight.options.mesh;
    int bone_index = g_vertex_weight.options.bone_index;
    for (int vertex_index=0; vertex_index<g_vertex_weight.options.vertex_count; vertex_index++) {
        AddVertexWeight(m, vertex_index, bone_index, delta);
    }
}

void BeginVertexWeightTool(const VertexWeightToolOptions& options) {
    static ToolVtable vtable = {
        .update = UpdateVertexWeightTool
    };

    BeginTool({
        .type = TOOL_TYPE_WEIGHT,
        .vtable = vtable,
        .input = g_view.input_tool,
        .inherit_input = true
    });

    g_vertex_weight = {};
    g_vertex_weight.options = options;

    MeshData* m = options.mesh;
    for (int vertex_index=0; vertex_index<options.vertex_count; vertex_index++)
        for (int weight_index=0; weight_index<MESH_MAX_VERTEX_WEIGHTS; weight_index++)
            g_vertex_weight.initial_weights[vertex_index * MESH_MAX_VERTEX_WEIGHTS + weight_index] =
                m->vertices[options.vertices[vertex_index]].weights[weight_index].weight;

    BeginDrag();
}
