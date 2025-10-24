//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

constexpr float ROTATE_TOOL_CENTER_SIZE = 0.2f;

struct RotateTool {
    float angle;
    float radius;
    RotateToolOptions options;
    Mesh* arc_mesh;
};

static RotateTool g_rotate = {};

static void EndRotate(bool commit) {
    if (commit && g_rotate.options.commit)
        g_rotate.options.commit(g_rotate.angle);
    else if (!commit && g_rotate.options.cancel)
        g_rotate.options.cancel();

    EndDrag();
    EndTool();

    if (g_rotate.arc_mesh) {
        Free(g_rotate.arc_mesh);
        g_rotate.arc_mesh = nullptr;
    }
}

static void UpdateArcMesh() {
    Free(g_rotate.arc_mesh);
    g_rotate.arc_mesh = nullptr;

    if (Abs(g_rotate.angle) > 0.01f) {
        PushScratch();
        MeshBuilder* builder = CreateMeshBuilder(ALLOCATOR_SCRATCH, 128, 384);
        if (g_rotate.angle < 0)
            AddArc(builder, VEC2_ZERO, g_rotate.radius, g_rotate.angle, 0.0f, 32, VEC2_ZERO);
        else
            AddArc(builder, VEC2_ZERO, g_rotate.radius, 0.0f, g_rotate.angle, 32, VEC2_ZERO);

        g_rotate.arc_mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE, true);
        Free(builder);
        PopScratch();
    }
}

static void UpdateRotate() {
    if (WasButtonPressed(GetInputSet(), MOUSE_LEFT)) {
        EndRotate(true);
        return;
    }

    if (!g_view.drag || WasButtonPressed(GetInputSet(), KEY_ESCAPE)) {
        EndRotate(false);
        return;
    }

    const Vec2& center = g_rotate.options.origin;
    Vec2 start_dir = g_view.drag_world_position - center;
    Vec2 current_dir = g_view.mouse_world_position - center;

    float radius = Length(current_dir);
    float start_angle = Atan2(start_dir.y, start_dir.x);
    float current_angle = Atan2(current_dir.y, current_dir.x);
    float angle = -Degrees(NormalizeAngle180(current_angle - start_angle));

    if (g_rotate.angle == angle && g_rotate.radius == radius)
        return;

    g_rotate.angle = angle;
    g_rotate.radius = radius;
    UpdateArcMesh();

    if (g_rotate.options.update)
        g_rotate.options.update(angle);
}

static void DrawRotate() {
    const Vec2& center = g_rotate.options.origin;
    const Vec2 dir = Normalize(g_view.drag_world_position - center);

    // Draw center point
    BindColor(SetAlpha(COLOR_CENTER, 0.75f));
    DrawVertex(center, ROTATE_TOOL_CENTER_SIZE * 0.75f);

    // Draw start line extending to current radius
    Vec2 start_end = center + dir * g_rotate.radius;
    BindColor(SetAlpha(COLOR_CENTER, 0.1f));
    DrawLine(center, start_end);

    // Draw current line
    BindColor(COLOR_CENTER);
    DrawDashedLine(center, g_view.mouse_world_position);

    if (g_rotate.arc_mesh) {
        BindColor(SetAlpha(COLOR_VERTEX, 0.1f));
        DrawMesh(g_rotate.arc_mesh, TRS(center, dir, VEC2_ONE));
    }

    BindColor(COLOR_ORIGIN);
    DrawVertex(g_view.mouse_world_position, ROTATE_TOOL_CENTER_SIZE);
}

void BeginRotate(const RotateToolOptions& options) {
    static ToolVtable vtable = {
        .update = UpdateRotate,
        .draw = DrawRotate,
    };

    BeginTool({
        .type = TOOL_TYPE_ROTATE,
        .vtable = vtable,
        .input = g_view.input_tool
    });

    g_rotate = {};
    g_rotate.options = options;

    BeginDrag();
}
