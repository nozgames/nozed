//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#include <editor.h>
#include <utils/file_helpers.h>

namespace fs = std::filesystem;

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

EditorAsset* CreateEditorAsset(const std::filesystem::path& path)
{
    EditorAsset* ea = (EditorAsset*)Alloc(g_editor.asset_allocator, sizeof(EditorAssetData));
    Copy(ea->path, sizeof(ea->path), path.string().c_str());
    ea->name = MakeCanonicalAssetName(fs::path(path).replace_extension("").filename().string().c_str());
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
    case EDITOR_ASSET_TYPE_MESH:
        InitEditorMesh(ea);
        break;

    case EDITOR_ASSET_TYPE_VFX:
        InitEditorVfx(ea);
        break;

    case EDITOR_ASSET_TYPE_ANIMATION:
        InitEditorAnimation(ea);
        break;

    case EDITOR_ASSET_TYPE_SKELETON:
        InitEditorSkeleton(ea);
        break;

    default:
        break;
    }

    return ea;
}

static void LoadAssetMetadata(EditorAsset* ea, const std::filesystem::path& path) {
    Props* props = LoadProps(std::filesystem::path(path.string() + ".meta"));
    if (!props)
        return;

    ea->position = props->GetVec2("editor", "position", VEC2_ZERO);
    ea->sort_order = props->GetInt("editor", "sort_order", 0);

    if (ea->vtable.load_metadata)
        ea->vtable.load_metadata(ea, props);
}

static void SaveAssetMetadata(EditorAsset* ea) {
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
        EditorAsset* ea = GetEditorAsset(i);
        if (!ea || (!ea->modified && !ea->meta_modified))
            continue;

        SaveAssetMetadata(ea);

        ea->meta_modified= false;
    }
}

void MoveTo(EditorAsset* ea, const Vec2& position)
{
    ea->position = position;
    ea->meta_modified = true;
}

void DrawSelectedEdges(EditorMesh* em, const Vec2& position)
{
    BindMaterial(g_view.vertex_material);

    for (i32 edge_index=0; edge_index < em->edge_count; edge_index++)
    {
        const EditorEdge& ee = em->edges[edge_index];
        if (!ee.selected)
            continue;

        const Vec2& v0 = em->vertices[ee.v0].position;
        const Vec2& v1 = em->vertices[ee.v1].position;
        DrawLine(v0 + position, v1 + position);
    }
}

void DrawEdges(EditorMesh* em, const Vec2& position)
{
    BindMaterial(g_view.vertex_material);

    for (i32 edge_index=0; edge_index < em->edge_count; edge_index++)
    {
        const EditorEdge& ee = em->edges[edge_index];
        DrawLine(em->vertices[ee.v0].position + position, em->vertices[ee.v1].position + position);
    }
}

void DrawSelectedFaces(EditorMesh* em, const Vec2& position)
{
    BindMaterial(g_view.vertex_material);

    for (i32 face_index=0; face_index < em->face_count; face_index++)
    {
        const EditorFace& ef = em->faces[face_index];
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

void DrawFaceCenters(EditorMesh* em, const Vec2& position)
{
    BindMaterial(g_view.vertex_material);
    for (int i=0; i<em->face_count; i++)
    {
        EditorFace& ef = em->faces[i];
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
        EditorAsset* ea = GetEditorAsset(i);;
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

bool OverlapPoint(EditorAsset* ea, const Vec2& overlap_point)
{
    if (!Contains(ea->bounds + ea->position, overlap_point))
        return false;

    if (!ea->vtable.overlap_point)
        return true;

    return ea->vtable.overlap_point(ea, ea->position, overlap_point);
}

bool OverlapPoint(EditorAsset* ea, const Vec2& position, const Vec2& overlap_point)
{
    if (!ea->vtable.overlap_point)
        return false;

    return ea->vtable.overlap_point(ea, position, overlap_point);
}

bool OverlapBounds(EditorAsset* ea, const Bounds2& overlap_bounds)
{
    if (!ea->vtable.overlap_bounds)
        return Intersects(ea->bounds + ea->position, overlap_bounds);

    return ea->vtable.overlap_bounds(ea, overlap_bounds);
}

int HitTestAssets(const Vec2& overlap_point)
{
    for (int i=MAX_ASSETS-1; i>=0; i--)
    {
        EditorAsset* ea = GetSortedEditorAsset(i);
        if (!ea)
            continue;

        if (OverlapPoint(ea, overlap_point))
            return i;
    }

    return -1;
}

int HitTestAssets(const Bounds2& hit_bounds)
{
    for (int i=0; i<MAX_ASSETS; i++)
    {
        EditorAsset* ea = GetEditorAsset(i);
        if (!ea)
            continue;

        if (OverlapBounds(ea, hit_bounds))
            return i;
    }

    return -1;
}

void DrawAsset(EditorAsset* ea)
{
    if (ea->vtable.draw)
        ea->vtable.draw(ea);
}

Bounds2 GetBounds(EditorAsset* ea)
{
    return ea->bounds;
}

int GetFirstSelectedAsset()
{
    for (u32 i=0; i<MAX_ASSETS; i++)
    {
        EditorAsset* ea = GetEditorAsset(i);
        if (ea && ea->selected)
            return i;
    }

    return -1;
}

void ClearAssetSelection()
{
    for (u32 i=0; i<MAX_ASSETS; i++)
    {
        EditorAsset* ea = GetEditorAsset(i);
        if (!ea)
            continue;
        ea->selected = false;
    }

    g_view.selected_asset_count = 0;
}

void SetAssetSelection(int asset_index)
{
    ClearAssetSelection();
    EditorAsset* ea = GetEditorAsset(asset_index);
    if (ea)
        ea->selected = true;
    g_view.selected_asset_count = 1;
}


void AddAssetSelection(int asset_index)
{
    EditorAsset* ea = GetEditorAsset(asset_index);
    if (!ea || ea->selected)
        return;

    ea->selected = true;
    g_view.selected_asset_count++;
}

EditorAsset* GetEditorAsset(EditorAssetType type, const Name* name)
{
    for (u32 i=0; i<MAX_ASSETS; i++)
    {
        EditorAsset* ea = GetEditorAsset(i);
        if (!ea)
            continue;

        if ((type == EDITOR_ASSET_TYPE_UNKNOWN || ea->type == type) && ea->name == name)
            return ea;
    }

    return nullptr;
}

void Clone(EditorAsset* dst, EditorAsset* src)
{
    *(EditorAssetData*)dst = *(EditorAssetData*)src;

    if (dst->vtable.clone)
        dst->vtable.clone((EditorAsset*)dst);
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

            EditorAsset* ea = nullptr;
            for (int asset_type=0; !ea && asset_type<EDITOR_ASSET_TYPE_COUNT; asset_type++)
                ea = CreateEditorAsset(asset_path);

            if (ea)
                LoadAssetMetadata(ea, asset_path);
        }
    }
}

void LoadEditorAsset(EditorAsset* ea)
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
        EditorAsset* ea = GetEditorAsset(asset_index);
        if (!ea)
            continue;

        LoadEditorAsset(ea);
    }

    for (u32 i=0; i<MAX_ASSETS; i++)
    {
        EditorAsset* ea = GetEditorAsset(i);
        if (ea && ea->vtable.post_load)
            ea->vtable.post_load(ea);
    }
}

void HotloadEditorAsset(const Name* name)
{
    for (u32 i=0; i<MAX_ASSETS; i++)
    {
        EditorAsset* ea = GetEditorAsset(i);
        if (!ea || ea->name != name)
            continue;

        switch (ea->type)
        {
        case EDITOR_ASSET_TYPE_VFX:
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
    MarkModified(GetEditingAsset());
}

void MarkModified(EditorAsset* ea)
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

int GetIndex(EditorAsset* ea)
{
    assert(ea);
    return GetIndex(g_editor.asset_allocator, ea);
}
