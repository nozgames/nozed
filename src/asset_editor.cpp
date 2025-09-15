//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#include <editor.h>
#include <utils/file_helpers.h>

EditorAsset* CreateEditorAsset(const std::filesystem::path& path, EditorAssetType type)
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

static void LoadAssetMetadata(EditorAsset& ea, const std::filesystem::path& path)
{
    Props* props = LoadProps(std::filesystem::path(path.string() + ".meta"));
    if (!props)
        return;

    ea.position = props->GetVec2("editor", "position", VEC2_ZERO);

    switch (ea.type)
    {
    case EDITOR_ASSET_TYPE_SKELETON:
        LoadAssetMetadata(*ea.skeleton, props);
        break;

    default:
        break;
    }
}

static void SaveAssetMetadata(const EditorAsset& ea)
{
    std::filesystem::path meta_path = std::filesystem::path(std::string(ea.path) + ".meta");
    Props* props = LoadProps(meta_path);
    if (!props)
        props = new Props{};
    props->SetVec2("editor", "position", ea.position);

    if (ea.vtable.save_metadata)
        ea.vtable.save_metadata(ea, props);

    SaveProps(props, meta_path);
}

static void SaveAssetMetadata()
{
    for (u32 i=0; i<g_view.asset_count; i++)
    {
        EditorAsset& asset = *g_view.assets[i];
        if (!asset.modified && !asset.meta_modified)
            continue;

        SaveAssetMetadata(asset);

        asset.meta_modified= false;
    }
}

void MoveTo(EditorAsset& asset, const Vec2& position)
{
    asset.position = position;
    asset.meta_modified = true;
}

void DrawEdges(const EditorAsset& ea, int min_edge_count, Color color)
{
    if (ea.type != EDITOR_ASSET_TYPE_MESH)
        return;

    BindColor(color);
    BindMaterial(g_view.vertex_material);

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

void SaveEditorAssets()
{
    SaveAssetMetadata();

    u32 count = 0;
    for (u32 i=0; i<g_view.asset_count; i++)
    {
        EditorAsset& ea = *g_view.assets[i];
        if (!ea.modified)
            continue;

        ea.modified = false;

        switch (ea.type)
        {
        case EDITOR_ASSET_TYPE_MESH:
            SaveEditorMesh(*ea.mesh, ea.path);
            break;

        case EDITOR_ASSET_TYPE_SKELETON:
            SaveEditorSkeleton(*ea.skeleton, ea.path);
            break;

        case EDITOR_ASSET_TYPE_ANIMATION:
            SaveEditorAnimation(*ea.anim, ea.path);
            break;

        default:
            continue;
        }

        count++;
    }

    if (count > 0)
        AddNotification("Saved %d asset(s)", count);
}

bool HitTestAsset(const EditorAsset& ea, const Vec2& hit_pos)
{
    return HitTestAsset(ea, ea.position, hit_pos);
}

bool HitTestAsset(const EditorAsset& ea, const Vec2& position, const Vec2& hit_pos)
{
    switch (ea.type)
    {
    case EDITOR_ASSET_TYPE_MESH:
        return -1 != HitTestTriangle(*ea.mesh, position, hit_pos);

    case EDITOR_ASSET_TYPE_VFX:
        return Contains(GetBounds(ea.vfx->vfx) + position, hit_pos);

    case EDITOR_ASSET_TYPE_SKELETON:
        return Contains(ea.skeleton->bounds + position, hit_pos);

    case EDITOR_ASSET_TYPE_ANIMATION:
        return Contains(ea.anim->bounds + position, hit_pos);

    default:
        return false;
    }
}

int HitTestAssets(const Vec2& hit_pos)
{
    for (int i=0, c=g_view.asset_count; i<c; i++)
        if (HitTestAsset(*g_view.assets[i], hit_pos))
            return i;

    return -1;
}

bool HitTestAsset(const EditorAsset& ea, const Bounds2& hit_bounds)
{
    switch (ea.type)
    {
    case EDITOR_ASSET_TYPE_MESH:
        return HitTest(*ea.mesh, ea.position, hit_bounds);

    case EDITOR_ASSET_TYPE_VFX:
        return Intersects(GetBounds(ea.vfx->vfx) + ea.position, hit_bounds);

    case EDITOR_ASSET_TYPE_SKELETON:
        return Intersects(ea.skeleton->bounds + ea.position, hit_bounds);

    case EDITOR_ASSET_TYPE_ANIMATION:
        return Intersects(ea.anim->bounds + ea.position, hit_bounds);

    default:
        return false;
    }
}

int HitTestAssets(const Bounds2& hit_bounds)
{
    for (int i=0, c=g_view.asset_count; i<c; i++)
        if (HitTestAsset(*g_view.assets[i], hit_bounds))
            return i;

    return -1;
}

void DrawAsset(EditorAsset& ea)
{
    if (ea.vtable.draw)
        ea.vtable.draw(ea);

    switch (ea.type)
    {
    case EDITOR_ASSET_TYPE_MESH:
        DrawEditorMesh(ea);
        break;

    case EDITOR_ASSET_TYPE_VFX:
        DrawEditorVfx(ea);
        break;

    case EDITOR_ASSET_TYPE_SKELETON:
        DrawEditorSkeleton(ea, ea.selected && !ea.editing);
        break;

    default:
        break;
    }
}

Bounds2 GetBounds(const EditorAsset& ea)
{
    switch (ea.type)
    {
    case EDITOR_ASSET_TYPE_MESH:
        return ea.mesh->bounds;

    case EDITOR_ASSET_TYPE_VFX:
        return GetBounds(ea.vfx->vfx);

    case EDITOR_ASSET_TYPE_SKELETON:
        return ea.skeleton->bounds;

    case EDITOR_ASSET_TYPE_ANIMATION:
        return ea.anim->bounds;

    default:
        break;
    }

    return { VEC2_ZERO, VEC2_ZERO };
}

Bounds2 GetSelectedBounds(const EditorAsset& ea)
{
    switch (ea.type)
    {
    case EDITOR_ASSET_TYPE_MESH:
        return GetSelectedBounds(*ea.mesh);

    case EDITOR_ASSET_TYPE_VFX:
        return GetBounds(ea.vfx->vfx);

    case EDITOR_ASSET_TYPE_SKELETON:
        return ea.skeleton->bounds;

    case EDITOR_ASSET_TYPE_ANIMATION:
        return ea.anim->bounds;

    default:
        break;
    }

    return { VEC2_ZERO, VEC2_ZERO };
}

int GetFirstSelectedAsset()
{
    for (u32 i=0; i<g_view.asset_count; i++)
        if (g_view.assets[i]->selected)
            return i;

    return -1;
}

void ClearAssetSelection()
{
    for (u32 i=0; i<g_view.asset_count; i++)
        g_view.assets[i]->selected = false;

    g_view.selected_asset_count = 0;
}

void SetAssetSelection(int asset_index)
{
    ClearAssetSelection();
    g_view.assets[asset_index]->selected = true;
    g_view.selected_asset_count = 1;
}


void AddAssetSelection(int asset_index)
{
    EditorAsset& ea = *g_view.assets[asset_index];
    if (ea.selected)
        return;

    ea.selected = true;
    g_view.selected_asset_count++;
}

int FindEditorAssetByName(const Name* name)
{
    for (u32 i=0; i<g_view.asset_count; i++)
        if (g_view.assets[i]->name == name)
            return i;

    return -1;
}

EditorAsset* Clone(Allocator* allocator, const EditorAsset& ea)
{
    EditorAsset* clone = CreateEditorAsset(ea.path, ea.type);
    *clone = ea;
    switch (clone->type)
    {
    case EDITOR_ASSET_TYPE_MESH:
        clone->mesh = Clone(allocator, *clone->mesh);
        break;
    case EDITOR_ASSET_TYPE_VFX:
        clone->vfx = Clone(allocator, *clone->vfx);
        break;
    default:
        break;
    }

    if (ea.vtable.clone)
        ea.vtable.clone(allocator, ea, *clone);

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
            ea = LoadEditorMeshAsset(asset_path);
        else if (ext == ".vfx")
            ea = LoadEditorVfxAsset(asset_path);
        else if (ext == ".skel")
            ea = LoadEditorSkeletonAsset(asset_path);
        else if (ext == ".anim")
            ea = LoadEditorAnimationAsset(asset_path);

        if (ea)
        {
            LoadAssetMetadata(*ea, asset_path);
            g_view.assets[g_view.asset_count++] = ea;
        }
    }

    for (u32 i=0; i<g_view.asset_count; i++)
    {
        EditorAsset& ea = *g_view.assets[i];
        if (ea.vtable.post_load)
            ea.vtable.post_load(ea);
    }
}

void HotloadEditorAsset(const Name* name)
{
    for (u32 i=0; i<g_view.asset_count; i++)
    {
        EditorAsset& ea = *g_view.assets[i];
        if (ea.name != name)
            continue;

        switch (ea.type)
        {
        case EDITOR_ASSET_TYPE_VFX:
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

void MarkModified(EditorAsset& ea)
{
    ea.modified = true;
}

std::filesystem::path GetEditorAssetPath(const Name* name, const char* ext)
{
    if (g_editor.asset_path_count == 0)
        return "";

    std::filesystem::path path;
    for (int p = 0; p<g_editor.asset_path_count; p++)
    {
        path = std::filesystem::current_path() / g_editor.asset_paths[p] / name->value;
        path += ext;
        if (std::filesystem::exists(path))
            break;
    }

    return path;
}

EditorAsset* GetEditorAsset(i32 index)
{
    if (index < 0 || index >= (i32)g_view.asset_count)
        return nullptr;

    return g_view.assets[index];
}
