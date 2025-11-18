//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

static void DrawSoundData(AssetData* a) {
    BindMaterial(g_view.shaded_material);
    BindColor(COLOR_WHITE);
    DrawMesh(MESH_ASSET_ICON_SOUND, Translate(a->position));
}

static void PlaySoundData(AssetData* a) {
    SoundData* s = static_cast<SoundData*>(a);
    if (!s->sound)
        s->sound = static_cast<Sound*>(LoadAssetInternal(ALLOCATOR_DEFAULT, s->name, ASSET_TYPE_SOUND, LoadSound));

    Play(s->sound, 1.0f, 1.0f);
}

static void InitSoundData(SoundData* s) {
    s->vtable = {
        .draw = DrawSoundData,
        .play = PlaySoundData
    };
}

void InitSoundData(AssetData* a) {
    assert(a);
    assert(a->type == ASSET_TYPE_SOUND);
    InitSoundData(static_cast<SoundData*>(a));
}
