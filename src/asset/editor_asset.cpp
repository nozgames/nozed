//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

constexpr Bounds2 VFX_BOUNDS = Bounds2{{-1.0f, -1.0f}, {1.0f, 1.0f}};

#include "../asset_editor/asset_editor.h"
#include "../utils/file_helpers.h"

EditorAsset* CreateEditableAsset(const std::filesystem::path& path, EditableAssetType type)
{
    std::error_code ec;
    std::filesystem::path relative_path = std::filesystem::relative(path, "assets", ec);
    relative_path.replace_extension("");
    relative_path = FixSlashes(relative_path);

    EditorAsset* ea = (EditorAsset*)Alloc(ALLOCATOR_DEFAULT, sizeof(EditorAsset));
    Copy(ea->path, sizeof(ea->path), path.string().c_str());
    ea->name = GetName(relative_path.string().c_str());
    ea->type = type;

    return ea;
}

EditorAsset* CreateEditableAsset(const std::filesystem::path& path, EditorMesh* em)
{
    EditorAsset* ea = CreateEditableAsset(path, EDITABLE_ASSET_TYPE_MESH);
    ea->mesh = em;
    return ea;
}

EditorAsset* CreateEditorMeshAsset(const std::filesystem::path& path)
{
    EditorMesh* em = LoadEditorMesh(ALLOCATOR_DEFAULT, path);
    if (!em)
        return nullptr;

    EditorAsset* ea = CreateEditableAsset(path, EDITABLE_ASSET_TYPE_MESH);
    ea->mesh = em;
    return ea;
}

static void ReadMetaData(EditorAsset& asset, const std::filesystem::path& path)
{
    Props* props = LoadProps(std::filesystem::path(path.string() + ".meta"));
    if (!props)
        return;

    asset.position = props->GetVec2("editor", "position", VEC2_ZERO);
}

static void SaveAssetMetaData(const EditorAsset& asset)
{
    std::filesystem::path meta_path = std::filesystem::path(std::string(asset.path) + ".meta");
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
        EditorAsset& asset = *g_asset_editor.assets[i];
        if (!asset.dirty)
            continue;

        SaveAssetMetaData(asset);
    }
}

void MoveTo(EditorAsset& asset, const Vec2& position)
{
    asset.position = position;
    asset.dirty = true;
}

void DrawEdges(const EditorAsset& ea, int min_edge_count, Color color)
{
    if (ea.type != EDITABLE_ASSET_TYPE_MESH)
        return;

    BindColor(color);
    BindMaterial(g_asset_editor.vertex_material);

    const EditorMesh& em = *ea.mesh;
    for (i32 edge_index=0; edge_index < em.edge_count; edge_index++)
    {
        const EditorEdge& ee = em.edges[edge_index];
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
        EditorAsset& asset = *g_asset_editor.assets[i];
        if (asset.type != EDITABLE_ASSET_TYPE_MESH)
            continue;

        if (!asset.mesh->modified)
            continue;

        SaveEditorMesh(*asset.mesh, asset.path);
        asset.mesh->modified = false;
        count++;
    }

    if (count > 0)
        AddNotification("Saved %d asset(s)", count);
}

bool HitTestAsset(const EditorAsset& ea, const Vec2& hit_pos)
{
    switch (ea.type)
    {
    case EDITABLE_ASSET_TYPE_MESH:
        return -1 != HitTestTriangle(*ea.mesh, ea.position, hit_pos);

    case EDITABLE_ASSET_TYPE_VFX:
        return Contains(VFX_BOUNDS + ea.position, hit_pos);

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

bool HitTestAsset(const EditorAsset& ea, const Bounds2& hit_bounds)
{
    switch (ea.type)
    {
    case EDITABLE_ASSET_TYPE_MESH:
        return HitTest(*ea.mesh, ea.position, hit_bounds);

    case EDITABLE_ASSET_TYPE_VFX:
        return Intersects(VFX_BOUNDS + ea.position, hit_bounds);

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

void DrawAsset(EditorAsset& ea)
{
    switch (ea.type)
    {
    case EDITABLE_ASSET_TYPE_MESH:
        BindTransform(TRS(ea.position, 0, VEC2_ONE));
        DrawMesh(ToMesh(*ea.mesh));
        break;

    case EDITABLE_ASSET_TYPE_VFX:
        if (!IsPlaying(ea.vfx_handle) && ea.vfx->vfx)
            ea.vfx_handle = Play(ea.vfx->vfx, ea.position);

        DrawOrigin(ea);
        break;

    default:
        break;
    }
}

Bounds2 GetBounds(const EditorAsset& ea)
{
    switch (ea.type)
    {
    case EDITABLE_ASSET_TYPE_MESH:
        return ea.mesh->bounds;

    case EDITABLE_ASSET_TYPE_VFX:
        return VFX_BOUNDS;

    default:
        break;
    }

    return { VEC2_ZERO, VEC2_ZERO };
}

Bounds2 GetSelectedBounds(const EditorAsset& ea)
{
    switch (ea.type)
    {
    case EDITABLE_ASSET_TYPE_MESH:
        return GetSelectedBounds(*ea.mesh);

    case EDITABLE_ASSET_TYPE_VFX:
        return VFX_BOUNDS;

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
    EditorAsset& ea = *g_asset_editor.assets[asset_index];
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

EditorAsset* Clone(Allocator* allocator, const EditorAsset& ea)
{
    EditorAsset* clone = CreateEditableAsset(ea.path, ea.type);
    *clone = ea;
    switch (clone->type)
    {
    case EDITABLE_ASSET_TYPE_MESH:
        clone->mesh = Clone(allocator, *clone->mesh);
        break;
    case EDITABLE_ASSET_TYPE_VFX:
        clone->vfx = Clone(allocator, *clone->vfx);
        break;
    default:
        break;
    }

    return clone;
}

void Copy(EditorAsset& dst, const EditorAsset& src)
{
    dst = src;

    if (dst.mesh)
        Copy(*dst.mesh, *src.mesh);
}

void LoadEditorAssets()
{
    for (auto& asset_path : GetFilesInDirectory("assets"))
    {
        std::filesystem::path ext = asset_path.extension();
        EditorAsset* ea = nullptr;

        if (ext == ".mesh")
            ea = CreateEditorMeshAsset(asset_path);
        else if (ext == ".vfx")
            ea = CreateEditorVfxAsset(asset_path);

        if (ea)
        {
            ReadMetaData(*ea, asset_path);
            g_asset_editor.assets[g_asset_editor.asset_count++] = ea;
        }
    }
}

void HotloadEditorAsset(const Name* name)
{
    for (int i=0; i<g_asset_editor.asset_count; i++)
    {
        EditorAsset& ea = *g_asset_editor.assets[i];
        if (ea.name != name)
            continue;

        switch (ea.type)
        {
        case EDITABLE_ASSET_TYPE_VFX:
            Stop(ea.vfx_handle);
            Free(ea.vfx);
            ea.vfx = LoadEditorVfx(ALLOCATOR_DEFAULT, ea.path);
            ea.vfx->vfx = ToVfx(ALLOCATOR_DEFAULT, *ea.vfx, ea.name);
            break;

        default:
            break;
        }
    }
}