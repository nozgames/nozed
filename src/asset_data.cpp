//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#include <editor.h>
#include <utils/file_helpers.h>

namespace fs = std::filesystem;

const Name* MakeCanonicalAssetName(const fs::path& path)
{
    return MakeCanonicalAssetName(fs::path(path).replace_extension("").filename().string().c_str());
}

const Name* MakeCanonicalAssetName(const char* name)
{
    std::string result = name;
    Lowercase(result.data(), (u32)result.size());
    Replace(result.data(), (u32)result.size(), '/', '_');
    Replace(result.data(), (u32)result.size(), '.', '_');
    Replace(result.data(), (u32)result.size(), ' ', '_');
    Replace(result.data(), (u32)result.size(), '-', '_');
    return GetName(result.c_str());
}

AssetData* CreateEditorAsset(const std::filesystem::path& path)
{
    AssetData* ea = (AssetData*)Alloc(g_editor.asset_allocator, sizeof(FatAssetData));
    Copy(ea->path, sizeof(ea->path), path.string().c_str());
    ea->name = MakeCanonicalAssetName(path);
    ea->bounds = Bounds2{{-0.5f, -0.5f}, {0.5f, 0.5f}};
    ea->asset_path_index = -1;

    for (int i=0; i<g_editor.asset_path_count; i++) {
        fs::path relative = path.lexically_relative(g_editor.asset_paths[i]);
        if (!relative.empty() && relative.string().find("..") == std::string::npos) {
            ea->asset_path_index = i;
            break;
        }
    }

    assert(ea->asset_path_index != -1);

    if (!InitImporter(ea))
    {
        Free(ea);
        return nullptr;
    }

    switch (ea->type) {
    case ASSET_TYPE_MESH:
        InitEditorMesh(ea);
        break;

    case ASSET_TYPE_VFX:
        InitEditorVfx(ea);
        break;

    case ASSET_TYPE_ANIMATION:
        InitEditorAnimation(ea);
        break;

    case ASSET_TYPE_SKELETON:
        InitEditorSkeleton(ea);
        break;

    default:
        break;
    }

    return ea;
}

static void LoadAssetMetadata(AssetData* ea, const std::filesystem::path& path) {
    Props* props = LoadProps(std::filesystem::path(path.string() + ".meta"));
    if (!props)
        return;

    ea->position = props->GetVec2("editor", "position", VEC2_ZERO);
    ea->sort_order = props->GetInt("editor", "sort_order", 0);

    if (ea->vtable.load_metadata)
        ea->vtable.load_metadata(ea, props);
}

static void SaveAssetMetadata(AssetData* ea) {
    std::filesystem::path meta_path = std::filesystem::path(std::string(ea->path) + ".meta");
    Props* props = LoadProps(meta_path);
    if (!props)
        props = new Props{};
    props->SetVec2("editor", "position", ea->position);
    props->SetInt("editor", "sort_order", ea->sort_order);

    if (ea->vtable.save_metadata)
        ea->vtable.save_metadata(ea, props);

    SaveProps(props, meta_path);
}

static void SaveAssetMetadata()
{
    for (u32 i=0; i<MAX_ASSETS; i++)
    {
        AssetData* ea = GetAssetData(i);
        if (!ea || (!ea->modified && !ea->meta_modified))
            continue;

        SaveAssetMetadata(ea);

        ea->meta_modified= false;
    }
}

void SetPosition(AssetData* ea, const Vec2& position)
{
    ea->position = position;
    ea->meta_modified = true;
}

void DrawSelectedEdges(MeshData* em, const Vec2& position)
{
    BindMaterial(g_view.vertex_material);

    for (i32 edge_index=0; edge_index < em->edge_count; edge_index++)
    {
        const EdgeData& ee = em->edges[edge_index];
        if (!ee.selected)
            continue;

        const Vec2& v0 = em->vertices[ee.v0].position;
        const Vec2& v1 = em->vertices[ee.v1].position;
        DrawLine(v0 + position, v1 + position);
    }
}

void DrawEdges(MeshData* em, const Vec2& position)
{
    BindMaterial(g_view.vertex_material);

    for (i32 edge_index=0; edge_index < em->edge_count; edge_index++)
    {
        const EdgeData& ee = em->edges[edge_index];
        DrawLine(em->vertices[ee.v0].position + position, em->vertices[ee.v1].position + position);
    }
}

void DrawSelectedFaces(MeshData* em, const Vec2& position)
{
    BindMaterial(g_view.vertex_material);

    for (i32 face_index=0; face_index < em->face_count; face_index++)
    {
        const FaceData& ef = em->faces[face_index];
        if (!ef.selected)
            continue;

        for (int vertex_index=0; vertex_index<ef.vertex_count; vertex_index++)
        {
            int v0 = em->face_vertices[ef.vertex_offset + vertex_index];
            int v1 = em->face_vertices[ef.vertex_offset + (vertex_index + 1) % ef.vertex_count];
            DrawLine(em->vertices[v0].position + position, em->vertices[v1].position + position);
        }
    }
}

void DrawFaceCenters(MeshData* em, const Vec2& position)
{
    BindMaterial(g_view.vertex_material);
    for (int i=0; i<em->face_count; i++)
    {
        FaceData& ef = em->faces[i];
        BindColor(ef.selected ? COLOR_VERTEX_SELECTED : COLOR_VERTEX);
        DrawVertex(position + GetFaceCenter(em, i));
    }
}

void SaveEditorAssets()
{
    SaveAssetMetadata();

    u32 count = 0;
    for (u32 i=0; i<MAX_ASSETS; i++)
    {
        AssetData* ea = GetAssetData(i);;
        if (!ea || !ea->modified)
            continue;

        ea->modified = false;

        if (ea->vtable.save)
            ea->vtable.save(ea, ea->path);
        else
            continue;

        count++;
    }

    if (count > 0)
        AddNotification("Saved %d asset(s)", count);
}

bool OverlapPoint(AssetData* ea, const Vec2& overlap_point)
{
    if (!Contains(ea->bounds + ea->position, overlap_point))
        return false;

    if (!ea->vtable.overlap_point)
        return true;

    return ea->vtable.overlap_point(ea, ea->position, overlap_point);
}

bool OverlapPoint(AssetData* ea, const Vec2& position, const Vec2& overlap_point)
{
    if (!ea->vtable.overlap_point)
        return false;

    return ea->vtable.overlap_point(ea, position, overlap_point);
}

bool OverlapBounds(AssetData* ea, const Bounds2& overlap_bounds)
{
    if (!ea->vtable.overlap_bounds)
        return Intersects(ea->bounds + ea->position, overlap_bounds);

    return ea->vtable.overlap_bounds(ea, overlap_bounds);
}

AssetData* HitTestAssets(const Vec2& overlap_point) {
    for (u32 i=GetAssetCount(); i>0; i--) {
        AssetData* a = GetSortedAssetData(i-1);
        if (!a)
            continue;

        if (OverlapPoint(a, overlap_point))
            return a;
    }

    return nullptr;
}

AssetData* HitTestAssets(const Bounds2& hit_bounds) {
    for (u32 i=GetAssetCount(); i>0; i--) {
        AssetData* a = GetSortedAssetData(i-1);
        if (!a)
            continue;

        if (OverlapBounds(a, hit_bounds))
            return a;
    }

    return nullptr;
}

void DrawAsset(AssetData* ea) {
    BindDepth(0.0f);
    if (ea->vtable.draw)
        ea->vtable.draw(ea);
}

AssetData* GetFirstSelectedAsset() {
    for (u32 i=0, c=GetAssetCount(); i<c; i++)
    {
        AssetData* a = GetSortedAssetData(i);
        assert(a);
        if (a->selected)
            return a;
    }

    return nullptr;
}

void ClearAssetSelection() {
    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* ea = GetSortedAssetData(i);
        assert(ea);
        ea->selected = false;
    }

    g_view.selected_asset_count = 0;
}

void SetSelected(AssetData* a, bool selected) {
    assert(a);
    if (a->selected == selected)
        return;
    a->selected = true;
    g_view.selected_asset_count++;
}

void ToggleSelected(AssetData* a) {
    assert(a);
    a->selected = !a->selected;
    if (a->selected)
        g_view.selected_asset_count++;
    else
        g_view.selected_asset_count--;
}

AssetData* GetAssetData(AssetType type, const Name* name)
{
    for (u32 i=0; i<MAX_ASSETS; i++)
    {
        AssetData* ea = GetAssetData(i);
        if (!ea)
            continue;

        if ((type == ASSET_TYPE_UNKNOWN || ea->type == type) && ea->name == name)
            return ea;
    }

    return nullptr;
}

void Clone(AssetData* dst, AssetData* src)
{
    *(FatAssetData*)dst = *(FatAssetData*)src;

    if (dst->vtable.clone)
        dst->vtable.clone((AssetData*)dst);
}

void InitEditorAssets()
{
    for (int i=0; i<g_editor.asset_path_count; i++)
    {
        std::vector<fs::path> asset_paths;
        GetFilesInDirectory(g_editor.asset_paths[i], asset_paths);

        for (auto& asset_path : asset_paths)
        {
            std::filesystem::path ext = asset_path.extension();
            if (ext == ".meta")
                continue;

            AssetData* ea = nullptr;
            for (int asset_type=0; !ea && asset_type<ASSET_TYPE_COUNT; asset_type++)
                ea = CreateEditorAsset(asset_path);

            if (ea)
                LoadAssetMetadata(ea, asset_path);
        }
    }
}

void LoadEditorAsset(AssetData* ea)
{
    assert(ea);

    if (ea->loaded)
        return;

    if (!ea->vtable.load)
        return;

    ea->loaded = true;
    ea->vtable.load(ea);
}

void LoadEditorAssets()
{
    for (int asset_index=0; asset_index<MAX_ASSETS; asset_index++)
    {
        AssetData* ea = GetAssetData(asset_index);
        if (!ea)
            continue;

        LoadEditorAsset(ea);
    }

    for (u32 i=0; i<MAX_ASSETS; i++)
    {
        AssetData* ea = GetAssetData(i);
        if (ea && ea->vtable.post_load)
            ea->vtable.post_load(ea);
    }
}

void HotloadEditorAsset(const Name* name)
{
    for (u32 i=0; i<MAX_ASSETS; i++)
    {
        AssetData* ea = GetAssetData(i);
        if (!ea || ea->name != name)
            continue;

        switch (ea->type)
        {
        case ASSET_TYPE_VFX:
            // Stop(ea->vfx_handle);
            // Free(ea->vfx);
            // ea->vfx = LoadEditorVfx(ALLOCATOR_DEFAULT, ea->path);
            // ea->vfx->vfx = ToVfx(ALLOCATOR_DEFAULT, *ea->vfx, ea->name);
            break;

        default:
            break;
        }
    }
}

void MarkModified()
{
    MarkModified(GetAssetData());
}

void MarkModified(AssetData* ea)
{
    ea->modified = true;
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

void DeleteAsset(AssetData* ea) {
    if (fs::exists(ea->path))
        fs::remove(ea->path);

    fs::path meta_path = fs::path(std::string(ea->path) + ".meta");
    if (fs::exists(meta_path))
        fs::remove(meta_path);

    Free(ea);
}

static int AssetSortFunc(const void* a, const void* b)
{
    int index_a = *(int*)a;
    int index_b = *(int*)b;

    AssetData* ea_a = GetAssetData(index_a);
    AssetData* ea_b = GetAssetData(index_b);
    if (!ea_a && !ea_b)
        return 0;

    if (!ea_a)
        return 1;

    if (!ea_b)
        return 0;

    if (ea_a->sort_order != ea_b->sort_order)
        return ea_a->sort_order - ea_b->sort_order;

    if (ea_a->type != ea_b->type)
        return ea_a->type - ea_b->type;

    return index_a - index_b;
}

void SortAssets() {
    u32 asset_index = 0;
    for (u32 i=0; i<MAX_ASSETS; i++) {
        AssetData* ea = GetAssetData(i);
        if (!ea)
            continue;

        g_editor.sorted_assets[asset_index++] = i;
    }

    qsort(g_editor.sorted_assets, asset_index, sizeof(int), AssetSortFunc);

    asset_index = 0;
    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* ea = GetSortedAssetData(i);
        if (!ea)
            continue;

        if (ea->sort_order != (int)asset_index * 10)
            ea->meta_modified = true;

        ea->sort_order = asset_index * 10;
        if (ea->vtable.on_sort_order_changed)
            ea->vtable.on_sort_order_changed(ea);

        asset_index++;
    }
}
