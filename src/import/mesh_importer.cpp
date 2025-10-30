//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//
// @STL

namespace fs = std::filesystem;
using namespace noz;

constexpr Vec2 OUTLINE_COLOR = ColorUV(0, 10);

struct OutlineConfig {
    float width;
    float offset;
    float boundary_taper;
};

static void ImportMesh(AssetData* a, Stream* output_stream, Props* config, Props* meta) {
    (void)config;
    (void)meta;

    assert(a);
    assert(a->type == ASSET_TYPE_MESH);
    MeshData* mesh_data = static_cast<MeshData*>(a);

    Mesh* m = ToMesh(mesh_data, false);

    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE;
    header.type = ASSET_TYPE_MESH;
    header.version = 1;
    WriteAssetHeader(output_stream, &header);

    WriteStruct(output_stream, mesh_data->bounds);
    WriteU16(output_stream, GetVertexCount(m));
    WriteU16(output_stream, GetIndexCount(m));

    const MeshVertex* v = GetVertices(m);
    WriteBytes(output_stream, v, sizeof(MeshVertex) * GetVertexCount(m));

    const u16* i = GetIndices(m);
    WriteBytes(output_stream, i, sizeof(u16) * GetIndexCount(m));
}

AssetImporter GetMeshImporter() {
    return {
        .type = ASSET_TYPE_MESH,
        .ext = ".mesh",
        .import_func = ImportMesh
    };
}

