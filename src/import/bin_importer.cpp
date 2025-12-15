//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

static void ImportBin(AssetData* a, const std::filesystem::path& path, Props* config, Props* meta) {
    (void)a;
    (void)config;
    (void)meta;

    Stream* stream = CreateStream(nullptr, 4096);

    AssetHeader header = {};
    header.type = ASSET_TYPE_BIN;
    header.version = 0;
    WriteAssetHeader(stream, &header);

    Stream* input_stream = LoadStream(ALLOCATOR_DEFAULT, a->path);
    WriteU32(stream, GetSize(input_stream));
    Copy(stream, input_stream);
    Free(input_stream);

    SaveStream(stream, path);
    Free(stream);
}

AssetImporter GetBinImporter() {
    return {
        .type = ASSET_TYPE_BIN,
        .ext = ".bin",
        .import_func = ImportBin
    };
}
