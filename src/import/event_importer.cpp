//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

static void ImportEvent(AssetData* a, Stream* output_stream, Props* config, Props* meta) {
    (void)a;
    (void)output_stream;
    (void)config;
    (void)meta;
}

AssetImporter GetEventImporter() {
    return {
        .type = ASSET_TYPE_EVENT,
        .ext = ".event",
        .import_func = ImportEvent
    };
}
