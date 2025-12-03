//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

static void ImportEvent(AssetData* a, Stream* stream, Props* config, Props* meta) {
    (void)a;
    (void)config;
    (void)meta;

    AssetHeader header = {};
    header.type = ASSET_TYPE_EVENT;
    header.version = 0;
    WriteAssetHeader(stream, &header);
}

AssetImporter GetEventImporter() {
    return {
        .type = ASSET_TYPE_EVENT,
        .ext = ".event",
        .import_func = ImportEvent
    };
}
