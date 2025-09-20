//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#include <editor.h>
#include <utils/file_helpers.h>

void AddEditorAsset(EditorAsset* ea)
{
    ea->index = g_view.asset_count;
    g_view.assets[g_view.asset_count++] = ea;
}

EditorAsset* CreateEditorAsset(Allocator* allocator, const std::filesystem::path& path, EditorAssetType type)
{
    std::error_code ec;
    std::filesystem::path relative_path = std::filesystem::relative(path, "assets", ec);
    relative_path.replace_extension("");
    relative_path = FixSlashes(relative_path);

    EditorAsset* ea = (EditorAsset*)Alloc(allocator, sizeof(EditorAsset));
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

    if (ea.vtable.load_metadata)
        ea.vtable.load_metadata(ea, props);
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

void DrawSelectedEdges(const EditorMesh& em, const Vec2& position)
{
    BindMaterial(g_view.vertex_material);

    for (i32 edge_index=0; edge_index < em.edge_count; edge_index++)
    {
        const EditorEdge& ee = em.edges[edge_index];
        if (!ee.selected)
            continue;

        const Vec2& v0 = em.vertices[ee.v0].position;
        const Vec2& v1 = em.vertices[ee.v1].position;
        DrawLine(v0 + position, v1 + position);
    }
}

void DrawEdges(const EditorMesh& em, const Vec2& position)
{
    BindMaterial(g_view.vertex_material);

    for (i32 edge_index=0; edge_index < em.edge_count; edge_index++)
    {
        const EditorEdge& ee = em.edges[edge_index];
        DrawLine(em.vertices[ee.v0].position + position, em.vertices[ee.v1].position + position);
    }
}

void DrawSelectedFaces(const EditorMesh& em, const Vec2& position)
{
    BindMaterial(g_view.vertex_material);

    for (i32 face_index=0; face_index < em.face_count; face_index++)
    {
        const EditorFace& ef = em.faces[face_index];
        if (!ef.selected)
            continue;

        Vec2 v0 = em.vertices[ef.v0].position + position;
        Vec2 v1 = em.vertices[ef.v1].position + position;
        Vec2 v2 = em.vertices[ef.v2].position + position;
        DrawLine(v0, v1);
        DrawLine(v1, v2);
        DrawLine(v2, v0);
    }
}

void DrawFaceCenters(EditorMesh& em, const Vec2& position)
{
    BindMaterial(g_view.vertex_material);
    for (int i=0; i<em.face_count; i++)
    {
        EditorFace& ef = em.faces[i];
        BindColor(ef.selected ? COLOR_VERTEX_SELECTED : COLOR_VERTEX);
        DrawVertex(position + GetFaceCenter(em, i));
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

        if (ea.vtable.save)
            ea.vtable.save(ea, ea.path);
        else
            continue;

        count++;
    }

    if (count > 0)
        AddNotification("Saved %d asset(s)", count);
}

bool OverlapPoint(EditorAsset& ea, const Vec2& overlap_point)
{
    if (!ea.vtable.overlap_point)
        return false;

    return ea.vtable.overlap_point(ea, ea.position, overlap_point);
}

bool OverlapPoint(EditorAsset& ea, const Vec2& position, const Vec2& overlap_point)
{
    if (!ea.vtable.overlap_point)
        return false;

    return ea.vtable.overlap_point(ea, position, overlap_point);
}

bool OverlapBounds(EditorAsset& ea, const Bounds2& overlap_bounds)
{
    if (!ea.vtable.overlap_bounds)
        return false;

    return ea.vtable.overlap_bounds(ea, overlap_bounds);
}

int HitTestAssets(const Vec2& overlap_point)
{
    for (int i=0, c=g_view.asset_count; i<c; i++)
        if (OverlapPoint(*g_view.assets[i], overlap_point))
            return i;

    return -1;
}

int HitTestAssets(const Bounds2& hit_bounds)
{
    for (int i=0, c=g_view.asset_count; i<c; i++)
        if (OverlapBounds(*g_view.assets[i], hit_bounds))
            return i;

    return -1;
}

void DrawAsset(EditorAsset& ea)
{
    if (ea.vtable.draw)
        ea.vtable.draw(ea);

    switch (ea.type)
    {

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

Bounds2 GetBounds(EditorAsset& ea)
{
    if (ea.vtable.bounds)
        return ea.vtable.bounds(ea);

    return BOUNDS2_ZERO;
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
    EditorAsset* clone = CreateEditorAsset(allocator, ea.path, ea.type);
    *clone = ea;
    if (clone->vtable.clone)
        clone->vtable.clone(*clone);

    return clone;
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
            AddEditorAsset(ea);
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
            // Stop(ea.vfx_handle);
            // Free(ea.vfx);
            // ea.vfx = LoadEditorVfx(ALLOCATOR_DEFAULT, ea.path);
            // ea.vfx->vfx = ToVfx(ALLOCATOR_DEFAULT, *ea.vfx, ea.name);
            break;

        default:
            break;
        }
    }
}

void MarkModified()
{
    MarkModified(GetEditingAsset());
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
