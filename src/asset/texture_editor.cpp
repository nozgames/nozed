//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

struct TextureEditor {
    InputSet* input;
    Shortcut* shortcuts;
    float saved_scale;
};

extern void DrawTextureData(AssetData* a);

static TextureEditor g_texture_editor = {};

static void BeginTextureEditor(AssetData*) {
    PushInputSet(g_texture_editor.input);
}

static void EndTextureEditor() {
    PopInputSet();
}

static void UpdateTextureEditor() {
    CheckShortcuts(g_texture_editor.shortcuts, g_texture_editor.input);
}

static void DrawTextureEditor() {
    TextureData* t = static_cast<TextureData*>(GetAssetData());
    const MeshVertex* vertices = GetVertices(g_view.quad_mesh);
    DrawTextureData(t);
    DrawBounds(t, 0, COLOR_VERTEX_SELECTED);
    Vec2 size = GetSize(t->bounds);
    DrawVertex(t->position + vertices[0].position * size);
    DrawVertex(t->position + vertices[1].position * size);
    DrawVertex(t->position + vertices[2].position * size);
    DrawVertex(t->position + vertices[3].position * size);
}

static void BeginTextureMove() {
}

static void UpdateTextureScaleTool(float scale) {
    TextureData* t = static_cast<TextureData*>(GetAssetData());
    t->scale = g_texture_editor.saved_scale * scale;
    UpdateBounds(t);
}

static void CommitTextureScaleTool(float) {
    TextureData* t = static_cast<TextureData*>(GetAssetData());
    MarkMetaModified(t);
    MarkModified(t);
}

static void CancelTextureScaleTool() {
    TextureData* t = static_cast<TextureData*>(GetAssetData());
    t->scale = g_texture_editor.saved_scale;
    UpdateBounds(t);
}

static void BeginTextureScale() {
    TextureData* t = static_cast<TextureData*>(GetAssetData());
    g_texture_editor.saved_scale = t->scale;
    BeginScaleTool({
        .origin = t->position,
        .update = UpdateTextureScaleTool,
        .commit = CommitTextureScaleTool,
        .cancel = CancelTextureScaleTool,
    });
}

void InitTextureEditor(TextureData* m) {
    m->vtable.editor_begin = m->editor_only ? BeginTextureEditor : nullptr;
    m->vtable.editor_end = EndTextureEditor;
    m->vtable.editor_update = UpdateTextureEditor;
    m->vtable.editor_draw = DrawTextureEditor;
}

void InitTextureEditor() {
    static Shortcut shortcuts[] = {
        { KEY_G, false, false, false, BeginTextureMove },
        { KEY_S, false, false, false, BeginTextureScale },
        { INPUT_CODE_NONE }
    };

    g_texture_editor.input = CreateInputSet(ALLOCATOR_DEFAULT);
    EnableCommonShortcuts(g_texture_editor.input);
    EnableButton(g_texture_editor.input, MOUSE_LEFT);
    EnableButton(g_texture_editor.input, MOUSE_SCROLL_Y);

    g_texture_editor.shortcuts = shortcuts;
    EnableShortcuts(shortcuts, g_texture_editor.input);
}
