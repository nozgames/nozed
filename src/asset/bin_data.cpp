//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

static void DrawBinData(AssetData* a) {
    BindMaterial(g_view.shaded_material);
    BindColor(COLOR_WHITE);
    DrawMesh(MESH_ASSET_ICON_BIN, Translate(a->position));
}

void InitBinData(AssetData* a) {
    assert(a);
    assert(a->type == ASSET_TYPE_BIN);
    BinData* b = static_cast<BinData*>(a);
    b->vtable = {
        .draw = DrawBinData,
    };
}

