//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

static void DrawShaderData(AssetData* a) {
    BindMaterial(g_view.shaded_material);
    BindColor(COLOR_WHITE);
    DrawMesh(MESH_ASSET_ICON_SHADER, Translate(a->position));
}

static void InitShaderData(ShaderData* a) {
    a->vtable = {
        .draw = DrawShaderData,
    };
}

void InitShaderData(AssetData* a) {
    assert(a);
    assert(a->type == ASSET_TYPE_SHADER);
    InitShaderData(static_cast<ShaderData*>(a));
}