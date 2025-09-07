//
//  MeshZ - Copyright(c) 2025 NoZ Games, LLC
//

constexpr float VERTEX_SIZE = 0.1f;
constexpr float REF_ZOOM = 10.0f;
constexpr Color VERTEX_COLOR = { 0.95f, 0.95f, 0.95f, 1.0f};
constexpr Color VERTEX_COLOR_SELECTED = { 1.0f, 0.788f, 0.055f, 1.0f};
constexpr Color EDGE_COLOR = { 0.25f, 0.25f, 0.25f, 1.0f};;

extern EditableMesh* CreateEditableMesh(Allocator* allocator);
extern Mesh* ToMesh(EditableMesh* emesh);
extern void DrawVertexHandles(EditableMesh* mesh);
extern void SetPosition(EditableMesh* emesh, int index, const Vec2& position);
extern int SplitEdge(EditableMesh* emesh, int edge_index, float edge_pos);
extern void DeleteVertex(EditableMesh* mesh, int vertex_index);
extern void DissolveVertex(EditableMesh* mesh, int vertex_index);
extern void RotateEdge(EditableMesh* mesh, int edge_index);
extern bool SaveEditableMesh(const EditableMesh* mesh, const char* filename);
extern EditableMesh* LoadEditableMesh(Allocator* allocator, const char* filename);
extern void SetTriangleColor(EditableMesh* emesh, int index, const Vec2Int& color);

struct View
{
    EditableMesh* emesh;
    Camera* camera;
    Material* material;
    Material* vertex_material;
    Mesh* vertex_mesh;
    Mesh* edge_mesh;
    float zoom;
    InputSet* input;
    int selected_vertex;
    Vec2 drag_start;
    Vec2 drag_position_start;
};

static int HitTestVertex(View* view)
{
    Vec2 mouse = GetMousePosition();
    Vec2 mouse_world = ScreenToWorld(view->camera, mouse);
    float zoom_scale = view->zoom / REF_ZOOM;

    for (int i=0; i<view->emesh->vertex_count; i++)
    {
        EditableVertex& ev = view->emesh->vertices[i];
        float dist = Length(mouse_world - ev.position);
        if (dist < VERTEX_SIZE * zoom_scale)
            return i;
    }

    return -1;
}

static int HitTestTriangle(View* view)
{
    Vec2 mouse = GetMousePosition();
    Vec2 mouse_world = ScreenToWorld(view->camera, mouse);

    for (int i=0; i<view->emesh->triangle_count; i++)
    {
        EditableTriangle& et = view->emesh->triangles[i];
        Vec2 v0 = view->emesh->vertices[et.v0].position;
        Vec2 v1 = view->emesh->vertices[et.v1].position;
        Vec2 v2 = view->emesh->vertices[et.v2].position;

        // Barycentric technique
        float area = 0.5f *(-v1.y * v2.x + v0.y * (-v1.x + v2.x) + v0.x * (v1.y - v2.y) + v1.x * v2.y);
        float s = 1/(2*area)*(v0.y*v2.x - v0.x*v2.y + (v2.y - v0.y)*mouse_world.x + (v0.x - v2.x)*mouse_world.y);
        float t = 1/(2*area)*(v0.x*v1.y - v0.y*v1.x + (v0.y - v1.y)*mouse_world.x + (v1.x - v0.x)*mouse_world.y);

        if (s >= 0 && t >= 0 && (s + t) <= 1)
            return i;
    }

    return -1;
}

static int HitTestEdge(View* view, float* pos)
{
    Vec2 mouse = GetMousePosition();
    Vec2 mouse_world = ScreenToWorld(view->camera, mouse);
    float zoom_scale = view->zoom / REF_ZOOM;

    for (int i=0; i<view->emesh->edge_count; i++)
    {
        EditableEdge& ee = view->emesh->edges[i];
        Vec2 v0 = view->emesh->vertices[ee.v0].position;
        Vec2 v1 = view->emesh->vertices[ee.v1].position;
        Vec2 edge_dir = Normalize(v1 - v0);
        Vec2 to_mouse = mouse_world - v0;
        float edge_length = Length(v1 - v0);
        float proj = Dot(to_mouse, edge_dir);
        if (proj >= 0 && proj <= edge_length)
        {
            Vec2 closest_point = v0 + edge_dir * proj;
            float dist = Length(mouse_world - closest_point);
            if (dist < VERTEX_SIZE * 0.5f * zoom_scale)
            {
                if (pos)
                    *pos = proj / edge_length;
                return i;
            }
        }
    }

    return -1;
}

static void UpdateCamera(View* view)
{
    SetExtents(view->camera, F32_MIN, F32_MIN, -view->zoom * 0.5f, view->zoom * 0.5f);
}

void UpdateView(View* view)
{
    if (WasButtonPressed(view->input, MOUSE_LEFT))
    {
        view->selected_vertex = HitTestVertex(view);
        if (view->selected_vertex != -1)
        {
            view->drag_start = GetMousePosition();
            view->drag_position_start = view->emesh->vertices[view->selected_vertex].position;
        }
    }

    if (WasButtonPressed(view->input, MOUSE_MIDDLE))
    {
        int edge = HitTestEdge(view, nullptr);
        if (edge != -1)
            RotateEdge(view->emesh, edge);
    }

    if (WasButtonPressed(view->input, MOUSE_RIGHT))
    {
        float edge_pos = 0.0f;
        int edge = HitTestEdge(view, &edge_pos);

        if (edge != -1)
        {
            int new_vertex = SplitEdge(view->emesh, edge, edge_pos);
            if (new_vertex != -1)
            {
                view->selected_vertex = new_vertex;
                view->drag_start = GetMousePosition();
                view->drag_position_start = view->emesh->vertices[view->selected_vertex].position;
            }
        }
    }

    if (WasButtonReleased(view->input, MOUSE_LEFT) || WasButtonReleased(view->input, MOUSE_RIGHT))
    {
        view->selected_vertex = -1;
    }

    if (WasButtonPressed(view->input, KEY_SPACE))
    {
        int triangle = HitTestTriangle(view);
        if (triangle != -1)
        {
            SetTriangleColor(view->emesh, triangle, Vec2Int{view->emesh->triangles[triangle].color.x + 1, 1});
        }
    }

    if (WasButtonPressed(view->input, KEY_ESCAPE))
    {
        SaveEditableMesh(view->emesh, "d:\\git\\nockerz\\assets\\meshes\\arrow.glb");
    }

    if (WasButtonPressed(view->input, KEY_X))
    {
        int vertex = HitTestVertex(view);
        if (vertex != -1)
            DissolveVertex(view->emesh, vertex);
    }

    if (view->selected_vertex != -1)
    {
        Vec2 drag_delta =
            ScreenToWorld(view->camera, GetMousePosition()) - ScreenToWorld(view->camera, view->drag_start);

        SetPosition(view->emesh, view->selected_vertex, view->drag_position_start + drag_delta);
    }

    float zoom_axis = GetAxis(view->input, MOUSE_SCROLL_Y);
    if (zoom_axis < -0.5f || zoom_axis > 0.5f)
    {
        view->zoom -= zoom_axis;
        UpdateCamera(view);
    }
}

void RenderView(View* view)
{
    BindCamera(view->camera);
    BindTransform(MAT3_IDENTITY);
    BindMaterial(view->material);
    DrawMesh(ToMesh(view->emesh));

    BindMaterial(view->vertex_material);
    BindColor(VERTEX_COLOR);

    float zoom_scale = view->zoom / REF_ZOOM;

    // Draw edges
    BindTransform(MAT3_IDENTITY);
    BindColor(EDGE_COLOR);
    for (int i=0; i<view->emesh->edge_count; i++)
    {
        Vec2 v0 = view->emesh->vertices[view->emesh->edges[i].v0].position;
        Vec2 v1 = view->emesh->vertices[view->emesh->edges[i].v1].position;
        Vec2 mid = (v0 + v1) * 0.5f;
        Vec2 dir = Normalize(v1 - v0);
        float length = Length(v1 - v0);
        BindTransform(TRS(mid, dir, Vec2{length * 0.5f, 0.01f * zoom_scale}));
        DrawMesh(view->edge_mesh);
    }

    // Draw verts
    BindColor(VERTEX_COLOR);
    for (int i=0; i<view->emesh->vertex_count; i++)
    {
        EditableVertex& ev = view->emesh->vertices[i];
        if (i == view->selected_vertex)
            continue;
        BindTransform(TRS(ev.position, 0, VEC2_ONE * zoom_scale));
        DrawMesh(view->vertex_mesh);
    }

    // Draw selected vert
    if (view->selected_vertex != -1)
    {
        EditableVertex& ev = view->emesh->vertices[view->selected_vertex];
        BindTransform(TRS(ev.position, 0, VEC2_ONE * zoom_scale));
        BindColor(VERTEX_COLOR_SELECTED);
        DrawMesh(view->vertex_mesh);
    }
}

View* CreateView(Allocator* allocator)
{
    View* view = (View*)Alloc(allocator, sizeof(View));
    //view->emesh = CreateEditableMesh(allocator);
    view->emesh = LoadEditableMesh(ALLOCATOR_DEFAULT, "d:\\git\\nockerz\\assets\\meshes\\arrow.glb");;

    view->camera = CreateCamera(allocator);
    view->material = CreateMaterial(allocator, g_assets.shaders._default);
    view->vertex_material = CreateMaterial(allocator, g_assets.shaders.ui);
    view->zoom = 20.0f;
    view->selected_vertex = -1;
        UpdateCamera(view);
        SetTexture(view->material, g_assets.textures.palette, 0);

    view->input = CreateInputSet(allocator);
    EnableButton(view->input, MOUSE_LEFT);
    EnableButton(view->input, MOUSE_RIGHT);
    EnableButton(view->input, MOUSE_MIDDLE);
    EnableButton(view->input, KEY_X);
    EnableButton(view->input, KEY_ESCAPE);
    EnableButton(view->input, KEY_SPACE);
    PushInputSet(view->input);

    MeshBuilder* builder = CreateMeshBuilder(ALLOCATOR_DEFAULT, 4, 6);
    AddVertex(builder, {                  0, -VERTEX_SIZE * 0.5f}, {0,0,1}, VEC2_ZERO, 0);
    AddVertex(builder, { VERTEX_SIZE * 0.5f,                   0}, {0,0,1}, VEC2_ZERO, 0);
    AddVertex(builder, {                  0,  VERTEX_SIZE * 0.5f}, {0,0,1}, VEC2_ZERO, 0);
    AddVertex(builder, {-VERTEX_SIZE * 0.5f,                   0}, {0,0,1}, VEC2_ZERO, 0);
    AddTriangle(builder, 0, 1, 2);
    AddTriangle(builder, 2, 3, 0);
    view->vertex_mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE);

    Clear(builder);
    AddVertex(builder, { -1, -1});
    AddVertex(builder, {  1, -1});
    AddVertex(builder, {  1,  1});
    AddVertex(builder, { -1,  1});
    AddTriangle(builder, 0, 1, 2);
    AddTriangle(builder, 2, 3, 0);
    view->edge_mesh = CreateMesh(ALLOCATOR_DEFAULT, builder, NAME_NONE);

    Free(builder);

    return view;
}

