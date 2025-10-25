//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

extern void InitTextureEditor(TextureData*);

void DrawTextureData(AssetData* a) {
    assert(a);
    assert(a->type == ASSET_TYPE_TEXTURE);

    TextureData* t = static_cast<TextureData*>(a);
    if (!t->texture) {
        t->texture = (Texture*)LoadAssetInternal(ALLOCATOR_DEFAULT, a->name, ASSET_SIGNATURE_TEXTURE, LoadTexture);
        t->material = CreateMaterial(ALLOCATOR_DEFAULT, SHADER_LIT);
        SetTexture(t->material, t->texture, 0);
    }

    BindColor(COLOR_WHITE);
    BindMaterial(t->material);
    DrawMesh(g_view.quad_mesh, Translate(a->position) * Scale(t->scale));
}

void UpdateBounds(TextureData* t) {
    t->bounds = Bounds2{
        Vec2{-0.5f, -0.5f} * t->scale,
        Vec2{0.5f, 0.5f} * t->scale
    };
}

static void LoadTextureMetaData(AssetData* a, Props* meta) {
    assert(a);
    assert(a->type == ASSET_TYPE_TEXTURE);
    TextureData* t = static_cast<TextureData*>(a);
    t->editor_only = meta->GetBool("texture", "reference", false);
    t->scale = meta->GetFloat("editor", "scale", 1.0f);
    InitTextureEditor(static_cast<TextureData*>(a));
    UpdateBounds(t);
}

static void SaveTextureMetaData(AssetData* a, Props* meta) {
    assert(a);
    assert(a->type == ASSET_TYPE_TEXTURE);
    TextureData* t = static_cast<TextureData*>(a);
    meta->SetString("editor", "scale", std::to_string(t->scale).c_str());
}

void InitTextureData(AssetData* a) {
    assert(a);
    assert(a->type == ASSET_TYPE_TEXTURE);

    TextureData* t = static_cast<TextureData*>(a);
    t->bounds = Bounds2{Vec2{-0.5f, -0.5f}, Vec2{0.5f, 0.5f}};
    t->scale = 1.0f;
    t->vtable = {
        .load_metadata = LoadTextureMetaData,
        .save_metadata = SaveTextureMetaData,
        .draw = DrawTextureData,
    };
}

