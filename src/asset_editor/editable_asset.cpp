//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"
#include "file_helpers.h"

extern EditableMesh* LoadEditableMesh(Allocator* allocator, const std::filesystem::path& filename);
extern bool SaveEditableMesh(const EditableMesh* mesh, const std::filesystem::path& filename);

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

static void SaveAssetMetaData()
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


void DrawEdges(const EditableAsset& ea, int min_edge_count, Color color)
{
    if (ea.type != EDITABLE_ASSET_TYPE_MESH)
        return;

    BindColor(color);
    BindMaterial(g_asset_editor.vertex_material);

    const EditableMesh& em = *ea.mesh;
    const f32 zoom_scale = g_asset_editor.zoom_ref_scale;
    for (i32 edge_index=0; edge_index < em.edge_count; edge_index++)
    {
        const EditableEdge& ee = em.edges[edge_index];
        if (ee.triangle_count > min_edge_count)
            continue;

        const Vec2& v0 = em.vertices[ee.v0].position;
        const Vec2& v1 = em.vertices[ee.v1].position;
        DrawLine(v0 + ea.position, v1 + ea.position, 0.01f);
    }
}

void SaveEditableAssets()
{
    SaveAssetMetaData();

    u32 count = 0;
    for (i32 i=0; i<g_asset_editor.asset_count; i++)
    {
        EditableAsset& asset = *g_asset_editor.assets[i];
        if (asset.type != EDITABLE_ASSET_TYPE_MESH)
            continue;

        if (!asset.mesh->modified)
            continue;

        SaveEditableMesh(asset.mesh, asset.path);
        asset.mesh->modified = false;
        count++;
    }

    if (count > 0)
        AddNotification("Saved %d asset(s)", count);
}

bool HitTestAsset(const EditableAsset& ea, const Vec2& hit_pos)
{
    switch (ea.type)
    {
    case EDITABLE_ASSET_TYPE_MESH:
        return -1 != HitTestTriangle(*ea.mesh, ea.position, hit_pos);

    default:
        return false;
    }
}

int HitTestAssets(const Vec2& hit_pos)
{
    for (int i=0, c=g_asset_editor.asset_count; i<c; i++)
        if (HitTestAsset(*g_asset_editor.assets[i], hit_pos))
            return i;

    return -1;
}

bool HitTestAsset(const EditableAsset& ea, const Bounds2& hit_bounds)
{
    switch (ea.type)
    {
    case EDITABLE_ASSET_TYPE_MESH:
        return HitTest(*ea.mesh, ea.position, hit_bounds);

    default:
        return false;
    }
}

int HitTestAssets(const Bounds2& hit_bounds)
{
    for (int i=0, c=g_asset_editor.asset_count; i<c; i++)
        if (HitTestAsset(*g_asset_editor.assets[i], hit_bounds))
            return i;

    return -1;
}

void DrawAsset(const EditableAsset& ea)
{
    switch (ea.type)
    {
    case EDITABLE_ASSET_TYPE_MESH:
        BindTransform(TRS(ea.position, 0, VEC2_ONE));
        DrawMesh(ToMesh(&*ea.mesh));
        break;

    default:
        break;
    }
}

Bounds2 GetBounds(const EditableAsset& ea)
{
    switch (ea.type)
    {
    case EDITABLE_ASSET_TYPE_MESH:
        return ea.mesh->bounds;

    default:
        break;
    }

    return { VEC2_ZERO, VEC2_ZERO };
}

Bounds2 GetSelectedBounds(const EditableAsset& ea)
{
    switch (ea.type)
    {
    case EDITABLE_ASSET_TYPE_MESH:
        return GetSelectedBounds(*ea.mesh);

    default:
        break;
    }

    return { VEC2_ZERO, VEC2_ZERO };
}

int GetFirstSelectedAsset()
{
    for (int i=0; i<g_asset_editor.asset_count; i++)
        if (g_asset_editor.assets[i]->selected)
            return i;

    return -1;
}

void ClearAssetSelection()
{
    for (int i=0; i<g_asset_editor.asset_count; i++)
        g_asset_editor.assets[i]->selected = false;

    g_asset_editor.selected_asset_count = 0;
}

void SetAssetSelection(int asset_index)
{
    ClearAssetSelection();
    g_asset_editor.assets[asset_index]->selected = true;
    g_asset_editor.selected_asset_count = 1;
}


void AddAssetSelection(int asset_index)
{
    EditableAsset& ea = *g_asset_editor.assets[asset_index];
    if (ea.selected)
        return;

    ea.selected = true;
    g_asset_editor.selected_asset_count++;
}

int FindAssetByName(const Name* name)
{
    for (int i=0; i<g_asset_editor.asset_count; i++)
        if (g_asset_editor.assets[i]->name == name)
            return i;

    return -1;
}
