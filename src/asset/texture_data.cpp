//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

extern void InitTextureEditor(TextureData*);

void DrawTextureData(AssetData* a) {
    assert(a);
    assert(a->type == ASSET_TYPE_TEXTURE);

    TextureData* t = static_cast<TextureData*>(a);
    if (!t)
        return;

    BindColor(COLOR_WHITE);
    BindMaterial(t->material);
    DrawMesh(g_view.quad_mesh, Translate(a->position) * Scale(Vec2{GetSize(t->bounds).x, GetSize(t->bounds).y}));
}

void UpdateBounds(TextureData* t) {
    t->bounds = Bounds2{
        Vec2{-0.5f, -0.5f} * t->scale,
        Vec2{0.5f, 0.5f} * t->scale
    };

    if (t->texture) {
        float aspect = (float)GetSize(t->texture).x / (float)GetSize(t->texture).y;
        if (aspect > 1.0f) {
            t->bounds.min.y *= 1.0f / aspect;
            t->bounds.max.y *= 1.0f / aspect;
        } else {
            t->bounds.min.x *= aspect;
            t->bounds.max.x *= aspect;
        }
    }
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

void PostLoadTextureData(AssetData* a) {
    assert(a->type == ASSET_TYPE_TEXTURE);
    TextureData* t = static_cast<TextureData*>(a);
    t->texture = (Texture*)LoadAssetInternal(ALLOCATOR_DEFAULT, a->name, ASSET_TYPE_TEXTURE, LoadTexture);
    t->material = CreateMaterial(ALLOCATOR_DEFAULT, SHADER_LIT);
    SetTexture(t->material, t->texture, 0);
    UpdateBounds(t);
}

static void ReloadTextureData(AssetData* a) {
    assert(a);
    assert(a->type == ASSET_TYPE_TEXTURE);

    TextureData* t = static_cast<TextureData*>(a);
    ReloadAsset(a->name, ASSET_TYPE_TEXTURE, t->texture, ReloadTexture);
}

void InitTextureData(AssetData* a) {
    assert(a);
    assert(a->type == ASSET_TYPE_TEXTURE);

    TextureData* t = static_cast<TextureData*>(a);
    t->bounds = Bounds2{Vec2{-0.5f, -0.5f}, Vec2{0.5f, 0.5f}};
    t->scale = 1.0f;
    t->vtable = {
        .reload = ReloadTextureData,
        .post_load = PostLoadTextureData,
        .load_metadata = LoadTextureMetaData,
        .save_metadata = SaveTextureMetaData,
        .draw = DrawTextureData,
    };
}

