//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

static void DrawFontData(AssetData* a) {
    BindMaterial(g_view.shaded_material);
    BindColor(COLOR_WHITE);
    DrawMesh(MESH_ASSET_ICON_FONT, Translate(a->position));
}

static void InitFontData(FontData* a) {
    a->vtable = {
        .draw = DrawFontData,
    };
}

void InitFontData(AssetData* a) {
    assert(a);
    assert(a->type == ASSET_TYPE_FONT);
    InitFontData(static_cast<FontData*>(a));
}
