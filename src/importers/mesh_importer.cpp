//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//
// @STL

#include "../gltf.h"
#include "../editor_mesh.h"
#include "../../../src/internal.h"

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

    EditorMesh* em = LoadEditorMesh(ALLOCATOR_SCRATCH, src_path);
    if (!em)
        throw std::runtime_error("invalid mesh");

    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE_MESH;
    header.version = 1;
    WriteAssetHeader(output_stream, &header);

    WriteStruct(output_stream, em->bounds);
    WriteU16(output_stream, (u16)(em->face_count * 3));
    WriteU16(output_stream, (u16)(em->face_count * 3));

    for (int i=0; i<em->face_count; i++)
    {
        const EditorFace& ef = em->faces[i];
        MeshVertex v = {};
        v.position = em->vertices[ef.v0].position;
        v.normal = VEC3_FORWARD;
        v.uv0 = VEC2_DOWN;
        v.bone = 0;
        WriteBytes(output_stream, &v, sizeof(MeshVertex));

        v.position = em->vertices[ef.v1].position;
        v.normal = VEC3_FORWARD;
        v.uv0 = VEC2_DOWN;
        v.bone = 0;
        WriteBytes(output_stream, &v, sizeof(MeshVertex));

        v.position = em->vertices[ef.v2].position;
        v.normal = VEC3_FORWARD;
        v.uv0 = VEC2_DOWN;
        v.bone = 0;
        WriteBytes(output_stream, &v, sizeof(MeshVertex));
    }

    for (int i=0; i<em->face_count; i++)
    {
        int v0 = i*3;
        WriteU16(output_stream, v0+0);
        WriteU16(output_stream, v0+1);
        WriteU16(output_stream, v0+2);
    }
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

