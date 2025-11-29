//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

constexpr float FRAME_SIZE_X = 20;
constexpr float FRAME_SIZE_Y = 40;
constexpr float FRAME_BORDER_SIZE = 1;
constexpr Color FRAME_BORDER_COLOR = Color24ToColor(32,32,32);
constexpr float FRAME_DOT_SIZE = 5;
constexpr float FRAME_DOT_OFFSET_X = FRAME_SIZE_X * 0.5f - FRAME_DOT_SIZE * 0.5f;
constexpr float FRAME_DOT_OFFSET_Y = 5;
constexpr Color FRAME_DOT_COLOR = FRAME_BORDER_COLOR;
constexpr Color FRAME_COLOR = Color32ToColor(100, 100, 100, 255);
constexpr Color FRAME_SELECTED_COLOR = COLOR_VERTEX_SELECTED;

struct AnimatedMeshEditor {
    AnimatedMeshData* data;
    Shortcut* shortcuts;
    float playback_time;
    AnimatedMesh* playing;
    MeshData clipboard;
    bool has_clipboard;
};

static AnimatedMeshEditor g_animated_mesh_editor = {};

inline AnimatedMeshData* GetAnimatedMeshData() {
    AssetData* a = g_animated_mesh_editor.data;
    assert(a->type == ASSET_TYPE_ANIMATED_MESH);
    return static_cast<AnimatedMeshData*>(a);
}

inline MeshData* GetAnimatedMeshFrameData(int frame_index) {
    AnimatedMeshData* m = GetAnimatedMeshData();
    assert(frame_index >= 0 && frame_index < m->frame_count);
    return &m->frames[frame_index];
}

inline MeshData* GetAnimatedMeshFrameData() {
    AnimatedMeshData* m = GetAnimatedMeshData();
    return &m->frames[m->current_frame];
}

static void DrawAnimatedMeshEditor() {
    AnimatedMeshData* m = GetAnimatedMeshData();
    MeshData* f = GetAnimatedMeshFrameData();

    if (g_animated_mesh_editor.playing) {
        AnimatedMesh* am = ToAnimatedMesh(m);
        BindColor(COLOR_WHITE);
        BindMaterial(g_view.shaded_material);
        DrawMesh(am, Translate(m->position), g_animated_mesh_editor.playback_time);
    } else {
        f->vtable.editor_draw();
    }

    int prev_frame = (m->current_frame - 1 + m->frame_count) % m->frame_count;
    if (prev_frame != m->current_frame) {
        MeshData* pf = GetAnimatedMeshFrameData(prev_frame);
        BindColor(COLOR_RED);
        BindMaterial(g_view.shaded_material);
        DrawEdges(pf, Translate(m->position));
    }

    int next_frame = (m->current_frame + 1) % m->frame_count;
    if (prev_frame != m->current_frame) {
        MeshData* pf = GetAnimatedMeshFrameData(next_frame);
        BindColor(COLOR_GREEN);
        BindMaterial(g_view.shaded_material);
        DrawEdges(pf, Translate(m->position));
    }
}

static void UpdateAnimatedMeshEditor() {
    AnimatedMeshData* m = static_cast<AnimatedMeshData*>(GetAssetData());
    MeshData* f = GetAnimatedMeshFrameData();
    if (f && f->modified) {
        MarkModified(m);
        f->modified = false;
    }

    if (g_animated_mesh_editor.playing) {
        AnimatedMesh* am = ToAnimatedMesh(m);
        g_animated_mesh_editor.playback_time = Update(am, g_animated_mesh_editor.playback_time, 1.0f, true);
        assert(FloorToInt(g_animated_mesh_editor.playback_time * ANIMATION_FRAME_RATE) < GetFrameCount(am));
    }

    CheckShortcuts(g_animated_mesh_editor.shortcuts, GetInputSet());

    f->vtable.editor_update();

    Canvas([m] {
        Align({.alignment=ALIGNMENT_BOTTOM_CENTER, .margin=EdgeInsetsBottom(60)}, [m] {
            Row([m] {
                int current_frame = m->current_frame;
                for (int frame_index=0; frame_index<m->frame_count; frame_index++) {
                    MeshData* f = &m->frames[frame_index];
                    Container({
                        .width=FRAME_SIZE_X * (1 + f->hold) + FRAME_BORDER_SIZE * 2,
                        .height=FRAME_SIZE_Y + FRAME_BORDER_SIZE * 2,
                        .margin=EdgeInsetsLeft(-2),
                        .color = frame_index == current_frame
                            ? FRAME_SELECTED_COLOR
                            : FRAME_COLOR,
                        .border = {.width=FRAME_BORDER_SIZE, .color=FRAME_BORDER_COLOR}
                    },
                    [] {
                        Align({.alignment=ALIGNMENT_BOTTOM_LEFT, .margin=EdgeInsetsBottomLeft(FRAME_DOT_OFFSET_Y, FRAME_DOT_OFFSET_X)}, [] {
                            Container({.width=FRAME_DOT_SIZE, .height=FRAME_DOT_SIZE, .color=FRAME_DOT_COLOR});
                        });
                    });
                }
            });
        });
    });
}

static void SetFrame(int frame_count) {
    AnimatedMeshData* m = GetAnimatedMeshData();
    if (m->current_frame != -1) {
        MeshData* f = &m->frames[m->current_frame];
        if (f->modified) {
            MarkModified(m);
            f->modified = false;
        }
        f->vtable.editor_end();
    }

    m->current_frame = Clamp(frame_count, 0, m->frame_count - 1);

    MeshData* f = &m->frames[m->current_frame];
    f->position = m->position;
    f->vtable.editor_begin(f);
}

static void BeginAnimatedMeshEditor(AssetData* a) {
    AnimatedMeshData* m = static_cast<AnimatedMeshData*>(a);
    m->current_frame = -1;
    g_animated_mesh_editor.data = m;
    SetFrame(0);
}

static void EndAnimatedMeshEditor() {
    AnimatedMeshData* m = GetAnimatedMeshData();
    if (m->current_frame != -1) {
        MeshData* f = &m->frames[m->current_frame];
        f->vtable.editor_end();
    }

    g_animated_mesh_editor.playing = nullptr;
}

void ShutdownAnimatedMeshEditor() {
    g_animated_mesh_editor.has_clipboard = false;
}

static void SetPrevFrame() {
    SetFrame((GetAnimatedMeshData()->current_frame - 1 + GetAnimatedMeshData()->frame_count) % GetAnimatedMeshData()->frame_count);
}

static void SetNextFrame() {
    SetFrame((GetAnimatedMeshData()->current_frame + 1) % GetAnimatedMeshData()->frame_count);
}

static void InsertFrameAfter() {
    AnimatedMeshData* m = GetAnimatedMeshData();
    MeshData* cf = GetAnimatedMeshFrameData(m->current_frame);
    m->frame_count++;

    for (int frame_index=m->frame_count - 1; frame_index > m->current_frame; frame_index--)
        m->frames[frame_index] = m->frames[frame_index - 1];

    MeshData* nf = GetAnimatedMeshFrameData(m->current_frame+1);
    InitMeshData(nf);
    *nf = *cf;
    nf->vtable.clone(nf);
    SetFrame(m->current_frame + 1);
    MarkModified(m);
}

static void TogglePlayAnimation() {
    if (g_animated_mesh_editor.playing) {
        Free(g_animated_mesh_editor.playing);
        g_animated_mesh_editor.playing = nullptr;
    } else {
        g_animated_mesh_editor.playing = ToAnimatedMesh(GetAnimatedMeshData());
        g_animated_mesh_editor.playback_time = 0.0f;
    }
}

static void IncHoldFrame() {
    GetAnimatedMeshFrameData()->hold++;
}

static void DecHoldFrame() {
    MeshData* f = GetAnimatedMeshFrameData();
    f->hold = Max(f->hold - 1, 0);
}

static void DeleteFrame() {
    AnimatedMeshData* m = GetAnimatedMeshData();
    if (m->frame_count <= 1)
        return;

    MeshData* cf = GetAnimatedMeshFrameData(m->current_frame);
    cf->vtable.editor_end();

    int deleted_frame = m->current_frame;
    for (int frame_index = deleted_frame; frame_index < m->frame_count - 1; frame_index++)
        m->frames[frame_index] = m->frames[frame_index + 1];

    m->frame_count--;
    m->current_frame = -1;
    SetFrame(Min(deleted_frame, m->frame_count - 1));
    MarkModified(m);
}

static void CopyFrame() {
    MeshData* f = GetAnimatedMeshFrameData();

    // Copy frame to clipboard
    InitMeshData(&g_animated_mesh_editor.clipboard);
    g_animated_mesh_editor.clipboard = *f;
    g_animated_mesh_editor.clipboard.vtable.clone(&g_animated_mesh_editor.clipboard);
    g_animated_mesh_editor.has_clipboard = true;
}

static void PasteFrame() {
    if (!g_animated_mesh_editor.has_clipboard)
        return;

    AnimatedMeshData* m = GetAnimatedMeshData();
    MeshData* f = GetAnimatedMeshFrameData();

    // End the current frame editor before replacing data
    f->vtable.editor_end();

    // Copy clipboard data to current frame
    MeshData* src = &g_animated_mesh_editor.clipboard;

    // Copy vertex data
    f->vertex_count = src->vertex_count;
    for (int i = 0; i < src->vertex_count; i++)
        f->vertices[i] = src->vertices[i];

    // Copy face data
    f->face_count = src->face_count;
    for (int i = 0; i < src->face_count; i++)
        f->faces[i] = src->faces[i];

    // Copy anchor data
    f->anchor_count = src->anchor_count;
    for (int i = 0; i < src->anchor_count; i++)
        f->anchors[i] = src->anchors[i];

    // Copy other properties
    f->edge_color = src->edge_color;
    f->opacity = src->opacity;
    f->depth = src->depth;

    // Rebuild edges and mark dirty
    UpdateEdges(f);
    MarkDirty(f);

    // Restart the frame editor
    f->position = m->position;
    f->vtable.editor_begin(f);

    MarkModified(m);
}

void InitAnimatedMeshEditor() {
    static Shortcut shortcuts[] = {
        { KEY_Q, false, false, false, SetPrevFrame },
        { KEY_E, false, false, false, SetNextFrame },
        { KEY_O, false, false, false, InsertFrameAfter },
        { KEY_SPACE, false, false, false, TogglePlayAnimation },
        { KEY_H, false, false, false, IncHoldFrame },
        { KEY_H, false, true, false, DecHoldFrame },
        { KEY_X, false, false, true, DeleteFrame },
        { KEY_C, false, true, false, CopyFrame },
        { KEY_V, false, true, false, PasteFrame },
        { INPUT_CODE_NONE }
    };

    g_animated_mesh_editor = {};
    g_animated_mesh_editor.shortcuts = shortcuts;
}

void InitAnimatedMeshEditor(AnimatedMeshData* m) {
    m->vtable.editor_begin = BeginAnimatedMeshEditor;
    m->vtable.editor_end = EndAnimatedMeshEditor;
    m->vtable.editor_update = UpdateAnimatedMeshEditor;
    m->vtable.editor_draw = DrawAnimatedMeshEditor;
}
