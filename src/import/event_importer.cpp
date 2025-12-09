//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

static void ImportEvent(AssetData* a, const std::filesystem::path& path, Props* config, Props* meta) {
    (void)a;
    (void)config;
    (void)meta;

    Stream* stream = CreateStream(nullptr, 4096);

    AssetHeader header = {};
    header.type = ASSET_TYPE_EVENT;
    header.version = 0;
    WriteAssetHeader(stream, &header);

    SaveStream(stream, path);
    Free(stream);
}

AssetImporter GetEventImporter() {
    return {
        .type = ASSET_TYPE_EVENT,
        .ext = ".event",
        .import_func = ImportEvent
    };
}
