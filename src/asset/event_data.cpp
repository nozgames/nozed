//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

static void DrawEventData(AssetData* a) {
    BindMaterial(g_view.shaded_material);
    BindColor(COLOR_WHITE);
    DrawMesh(MESH_ASSET_ICON_EVENT, Translate(a->position));
}

static void LoadEventData(AssetData* a) {
    assert(a);
    assert(a->type == ASSET_TYPE_EVENT);
    EventData* e = static_cast<EventData*>(a);

    std::string contents = ReadAllText(ALLOCATOR_DEFAULT, a->path);
    Props* props = Props::Load(contents.c_str(), contents.size());
    e->id = props->GetInt("event", "id", 0);
}

static EventData* LoadEventData(const std::filesystem::path& path) {
    std::string contents = ReadAllText(ALLOCATOR_DEFAULT, path);
    Tokenizer tk;
    Init(tk, contents.c_str());

    EventData* e = static_cast<EventData*>(CreateAssetData(path));
    assert(e);
    InitEventData(e);
    LoadEventData(e);
    return e;
}

AssetData* NewEventData(const std::filesystem::path& path) {
    constexpr const char* default_event = "\n";

    std::filesystem::path full_path = path.is_relative() ?  std::filesystem::path(g_editor.project_path) / "assets" / "events" / path : path;
    full_path += ".event";

    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);
    WriteCSTR(stream, default_event);
    SaveStream(stream, full_path);
    Free(stream);

    return LoadEventData(full_path);
}

void InitEventData(AssetData* a) {
    assert(a);
    assert(a->type == ASSET_TYPE_EVENT);
    EventData* e = static_cast<EventData*>(a);
    e->editor_only = true;
    e->vtable = {
        .load = LoadEventData,
        .draw = DrawEventData,
    };
}

