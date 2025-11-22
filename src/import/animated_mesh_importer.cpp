//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

static void ImportAnimatedMesh(AssetData* a, Stream* output_stream, Props* config, Props* meta) {
    (void)config;
    (void)meta;

    assert(a);
    assert(a->type == ASSET_TYPE_ANIMATED_MESH);
    MeshData* mesh_data = static_cast<MeshData*>(a);

    //Mesh* m = ToMesh(mesh_data, false);

    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE;
    header.type = ASSET_TYPE_ANIMATED_MESH;
    header.version = 1;
    WriteAssetHeader(output_stream, &header);

    WriteStruct(output_stream, mesh_data->bounds);
}

AssetImporter GetAnimatedMeshImporter() {
    return {
        .type = ASSET_TYPE_ANIMATED_MESH,
        .ext = ".anim_mesh",
        .import_func = ImportAnimatedMesh
    };
}

