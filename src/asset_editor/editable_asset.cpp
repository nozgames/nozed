//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"
#include "file_helpers.h"

extern EditableMesh* LoadEditableMesh(Allocator* allocator, const std::filesystem::path& filename);
extern int HitTest(const EditableMesh& mesh, const Vec2& position, const Vec2& hit_pos);

static EditableAsset* CreateEditableAsset(const std::filesystem::path& path, EditableAssetType type)
{
    std::error_code ec;
    std::filesystem::path relative_path = std::filesystem::relative(path, "assets", ec);
    relative_path.replace_extension("");
    relative_path = FixSlashes(relative_path);

    EditableAsset* ea = (EditableAsset*)Alloc(ALLOCATOR_DEFAULT, sizeof(EditableAsset));
    ea->path = path;
    ea->name = GetName(relative_path.string().c_str());
    ea->type = type;
    return ea;
}

static EditableAsset* CreateEditableMeshAsset(const std::filesystem::path& path)
{
    EditableAsset* ea = CreateEditableAsset(path, EDITABLE_ASSET_TYPE_MESH);
    ea->mesh = LoadEditableMesh(ALLOCATOR_DEFAULT, path);
    if (!ea->mesh)
    {
        Free(ea);
        ea = nullptr;
    }
    return ea;
}

static void ReadMetaData(EditableAsset& asset, const std::filesystem::path& path)
{
    Props* props = LoadProps(std::filesystem::path(path.string() + ".meta"));
    if (!props)
        return;

    asset.position = props->GetVec2("editor", "position", VEC2_ZERO);
}

int HitTestAssets(const Vec2& hit_pos)
{
    for (int i=0, c=g_asset_editor.asset_count; i<c; i++)
    {
        EditableAsset* ea = g_asset_editor.assets[i];
        const EditableMesh& em = *ea->mesh;
        const Vec2& position = ea->position;
        if (-1 != HitTest(em, position, hit_pos))
            return i;
    }

    return -1;
}

i32 LoadEditableAssets(EditableAsset** assets)
{
    i32 asset_count = 0;
    for (auto& asset_path : GetFilesInDirectory("assets"))
    {
        std::filesystem::path ext = asset_path.extension();
        EditableAsset* ea = nullptr;

        if (ext == ".glb")
            ea = CreateEditableMeshAsset(asset_path);

        if (ea)
        {
            assets[asset_count++] = ea;
            ReadMetaData(*ea, asset_path);

            if (asset_count > MAX_ASSETS)
                return asset_count;
        }
    }

    g_asset_editor.asset_count = asset_count;

    return asset_count;
}

static void SaveAssetMetaData(const EditableAsset& asset)
{
    std::filesystem::path meta_path = std::filesystem::path(asset.path.string() + ".meta");
    Props* props = LoadProps(meta_path);
    if (!props)
        props = new Props{};
    props->SetVec2("editor", "position", asset.position);
    SaveProps(props, meta_path);
}

void SaveAssetMetaData()
{
    for (i32 i=0; i<g_asset_editor.asset_count; i++)
    {
        EditableAsset& asset = *g_asset_editor.assets[i];
        if (!asset.dirty)
            continue;

        SaveAssetMetaData(asset);
    }
}

void MoveTo(EditableAsset& asset, const Vec2& position)
{
    asset.position = position;
    asset.dirty = true;
}

void DrawEdges(const EditableAsset& ea, int min_edge_count, float zoom_scale, Color color)
{
    if (ea.type != EDITABLE_ASSET_TYPE_MESH)
        return;

    BindColor(color);
    BindMaterial(g_asset_editor.vertex_material);

    const EditableMesh& em = *ea.mesh;
    for (i32 edge_index=0; edge_index < em.edge_count; edge_index++)
    {
        const EditableEdge& ee = em.edges[edge_index];
        if (ee.triangle_count > min_edge_count)
            continue;

        const Vec2& v0 = em.vertices[ee.v0].position;
        const Vec2& v1 = em.vertices[ee.v1].position;
        Vec2 mid = (v0 + v1) * 0.5f;
        Vec2 dir = Normalize(v1 - v0);
        float length = Length(v1 - v0);
        BindTransform(TRS(mid + ea.position, dir, Vec2{length * 0.5f, 0.01f * zoom_scale}));
        DrawMesh(g_asset_editor.edge_mesh);
    }
}
