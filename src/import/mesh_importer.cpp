//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//
// @STL

#include "../../../src/internal.h"
#include "../asset/editor_mesh.h"

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
    const fs::path& src_path = source_path;

    EditorMesh* em = LoadEditorMesh(ALLOCATOR_DEFAULT, src_path);
    if (!em)
        throw std::runtime_error("invalid mesh");

    Mesh* m = ToMesh(*em, false);

    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE_MESH;
    header.version = 1;
    WriteAssetHeader(output_stream, &header);

    WriteStruct(output_stream, em->bounds);
    WriteU16(output_stream, (u16)GetVertexCount(m));
    WriteU16(output_stream, (u16)GetIndexCount(m));

    const MeshVertex* v = GetVertices(m);
    for (int i=0, c=GetVertexCount(m); i<c; i++)
    {
        MeshVertex t = v[i];
        t.position.y = -t.position.y;
        WriteBytes(output_stream, &t, sizeof(MeshVertex));
    }

    const u16* i = GetIndices(m);
    WriteBytes(output_stream, i, sizeof(u16) * GetIndexCount(m));
}

bool CanImportMesh(const fs::path& source_path)
{
    Stream* stream = LoadStream(ALLOCATOR_DEFAULT, fs::path(source_path.string() + ".meta"));
    if (!stream)
        return true;
    Props* props = Props::Load(stream);
    if (!props)
    {
        Free(stream);
        return true;
    }

    bool can_import = !props->GetBool("mesh", "skip_mesh", false);
    Free(stream);
    delete props;
    return can_import;
}

static const char* g_mesh_extensions[] = { ".mesh", nullptr };

static AssetImporterTraits g_mesh_importer_traits = {
    .type_name = "Mesh",
    .signature = ASSET_SIGNATURE_MESH,
    .file_extensions = g_mesh_extensions,
    .import_func = ImportMesh,
    .can_import = CanImportMesh
};

AssetImporterTraits* GetMeshImporterTraits()
{
    return &g_mesh_importer_traits;
}

