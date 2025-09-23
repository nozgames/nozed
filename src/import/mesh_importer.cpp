//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//
// @STL

#include "../../../src/internal.h"
#include "../asset/mesh_editor.h"

namespace fs = std::filesystem;
using namespace noz;

constexpr Vec2 OUTLINE_COLOR = ColorUV(0, 10);

struct OutlineConfig
{
    float width;
    float offset;
    float boundary_taper;
};

void ImportMesh(const fs::path& source_path, Stream* output_stream, Props* config, Props* meta)
{
    (void)config;
    (void)meta;

    EditorMesh* em = LoadEditorMesh(source_path);
    if (!em)
        throw std::runtime_error("invalid mesh");

    Mesh* m = ToMesh(em, false);

    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE_MESH;
    header.version = 1;
    WriteAssetHeader(output_stream, &header);

    WriteStruct(output_stream, em->bounds);
    WriteU16(output_stream, GetVertexCount(m));
    WriteU16(output_stream, GetIndexCount(m));

    const MeshVertex* v = GetVertices(m);
    WriteBytes(output_stream, v, sizeof(MeshVertex) * GetVertexCount(m));

    const u16* i = GetIndices(m);
    WriteBytes(output_stream, i, sizeof(u16) * GetIndexCount(m));
}

static AssetImporterTraits g_mesh_importer_traits = {
    .signature = ASSET_SIGNATURE_MESH,
    .ext = ".mesh",
    .import_func = ImportMesh
};

AssetImporterTraits* GetMeshImporterTraits()
{
    return &g_mesh_importer_traits;
}

