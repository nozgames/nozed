//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

static void ImportAnimatedMesh(AssetData* a, Stream* stream, Props* config, Props* meta) {
    (void)config;
    (void)meta;

    assert(a);
    assert(a->type == ASSET_TYPE_ANIMATED_MESH);
    AnimatedMeshData* m = static_cast<AnimatedMeshData*>(a);

    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE;
    header.type = ASSET_TYPE_ANIMATED_MESH;
    header.version = 1;
    WriteAssetHeader(stream, &header);
    WriteStruct(stream, m->bounds);
    WriteU8(stream, (u8)ANIMATION_FRAME_RATE);
    WriteU8(stream, (u8)m->frame_count);
    for (int i=0; i<m->frame_count; i++) {
        MeshData* frame = &m->frames[i];
        SerializeMesh(ToMesh(frame, false), stream);
    }
}

AssetImporter GetAnimatedMeshImporter() {
    return {
        .type = ASSET_TYPE_ANIMATED_MESH,
        .ext = ".amesh",
        .import_func = ImportAnimatedMesh
    };
}

