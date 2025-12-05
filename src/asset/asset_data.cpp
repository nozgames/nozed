//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

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

static void DestroyAssetData(void* p) {
    AssetData* a = static_cast<AssetData*>(p);
    if (a->vtable.destructor) {
        a->vtable.destructor(a);
    }
}

AssetData* CreateAssetData(const std::filesystem::path& path) {
    AssetData* a = (AssetData*)Alloc(g_editor.asset_allocator, sizeof(FatAssetData), DestroyAssetData);
    Copy(a->path, sizeof(a->path), canonical(path).string().c_str());
    Lowercase(a->path, sizeof(a->path));
    a->name = MakeCanonicalAssetName(path);
    a->bounds = Bounds2{{-0.5f, -0.5f}, {0.5f, 0.5f}};
    a->asset_path_index = -1;

    for (int i=0; i<g_editor.asset_path_count; i++) {
        if (Equals(g_editor.asset_paths[i], a->path, Length(g_editor.asset_paths[i]), true)) {
            a->asset_path_index = i;
            break;
        }
    }

    assert(a->asset_path_index != -1);

    if (!InitImporter(a)) {
        Free(a);
        return nullptr;
    }

    if (a->type == ASSET_TYPE_TEXTURE)
        InitTextureData(a);
    else if (a->type == ASSET_TYPE_MESH)
        InitMeshData(a);
    else if (a->type == ASSET_TYPE_VFX)
        InitVfxData(a);
    else if (a->type == ASSET_TYPE_ANIMATION)
        InitAnimationData(a);
    else if (a->type == ASSET_TYPE_SKELETON)
        InitSkeletonData(a);
    else if (a->type == ASSET_TYPE_SHADER)
        InitShaderData(a);
    else if (a->type == ASSET_TYPE_SOUND)
        InitSoundData(a);
    else if (a->type == ASSET_TYPE_FONT)
        InitFontData(a);
    else if (a->type == ASSET_TYPE_ANIMATED_MESH)
        InitAnimatedMeshData(a);
    else if (a->type == ASSET_TYPE_EVENT)
        InitEventData(a);

    return a;
}

static void LoadAssetMetadata(AssetData* ea, const std::filesystem::path& path) {
    Props* props = LoadProps(std::filesystem::path(path.string() + ".meta"));
    if (!props)
        return;

    ea->position = props->GetVec2("editor", "position", VEC2_ZERO);

    if (ea->vtable.load_metadata)
        ea->vtable.load_metadata(ea, props);
}

static void SaveAssetMetadata(AssetData* a) {
    std::filesystem::path meta_path = std::filesystem::path(std::string(a->path) + ".meta");
    Props* props = LoadProps(meta_path);
    if (!props)
        props = new Props{};
    props->SetVec2("editor", "position", a->position);

    if (a->vtable.save_metadata)
        a->vtable.save_metadata(a, props);

    SaveProps(props, meta_path);
}

static void SaveAssetMetadata() {
    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* a = GetAssetData(i);
        assert(a);
        if (!a->modified && !a->meta_modified)
            continue;

        SaveAssetMetadata(a);

        a->meta_modified= false;
    }
}

void SetPosition(AssetData* a, const Vec2& position) {
    a->position = position;
    a->meta_modified = true;
}

void DrawSelectedEdges(MeshData* m, const Vec2& position) {
    BindMaterial(g_view.vertex_material);

    for (i32 edge_index=0; edge_index < m->edge_count; edge_index++) {
        const EdgeData& ee = m->edges[edge_index];
        if (!ee.selected)
            continue;

        const Vec2& v0 = m->vertices[ee.v0].position;
        const Vec2& v1 = m->vertices[ee.v1].position;
        DrawLine(v0 + position, v1 + position);
    }
}

void DrawEdges(MeshData* m, const Vec2& position) {
    BindMaterial(g_view.vertex_material);

    for (i32 edge_index=0; edge_index < m->edge_count; edge_index++) {
        const EdgeData& ee = m->edges[edge_index];
        DrawLine(m->vertices[ee.v0].position + position, m->vertices[ee.v1].position + position);
    }
}

void DrawEdges(MeshData* m, const Mat3& transform) {
    BindMaterial(g_view.vertex_material);

    for (i32 edge_index=0; edge_index < m->edge_count; edge_index++) {
        const EdgeData& ee = m->edges[edge_index];
        Vec2 p1 = TransformPoint(transform, m->vertices[ee.v0].position);
        Vec2 p2 = TransformPoint(transform, m->vertices[ee.v1].position);
        DrawLine(p1, p2);
    }
}

void DrawSelectedFaces(MeshData* m, const Vec2& position) {
    BindMaterial(g_view.vertex_material);

    for (i32 face_index=0; face_index < m->face_count; face_index++) {
        const FaceData& f = m->faces[face_index];
        if (!f.selected)
            continue;

        for (int vertex_index=0; vertex_index<f.vertex_count; vertex_index++) {
            int v0 = f.vertices[vertex_index];
            int v1 = f.vertices[(vertex_index + 1) % f.vertex_count];
            DrawLine(m->vertices[v0].position + position, m->vertices[v1].position + position);
        }
    }
}

void DrawFaceCenters(MeshData* m, const Vec2& position) {
    BindMaterial(g_view.vertex_material);
    for (int i=0; i<m->face_count; i++)
    {
        FaceData& ef = m->faces[i];
        BindColor(ef.selected ? COLOR_VERTEX_SELECTED : COLOR_VERTEX);
        DrawVertex(position + GetFaceCenter(m, i));
    }
}

void SaveAssetData() {
    SaveAssetMetadata();

    u32 count = 0;
    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* a = GetAssetData(i);;
        if (!a || !a->modified)
            continue;

        a->modified = false;

        if (a->vtable.save)
            a->vtable.save(a, a->path);
        else
            continue;

        count++;
    }

    if (count > 0)
        AddNotification(NOTIFICATION_TYPE_INFO, "Saved %d asset(s)", count);
}

bool OverlapPoint(AssetData* a, const Vec2& overlap_point) {
    return Contains(a->bounds + a->position, overlap_point);
}

bool OverlapPoint(AssetData* a, const Vec2& position, const Vec2& overlap_point) {
    return Contains(a->bounds + position, overlap_point);
}

bool OverlapBounds(AssetData* a, const Bounds2& bounds) {
    return Intersects(a->bounds + a->position, bounds);
}

AssetData* HitTestAssets(const Vec2& overlap_point) {
    AssetData* first_hit = nullptr;
    for (u32 i=GetAssetCount(); i>0; i--) {
        AssetData* a = GetAssetData(i-1);
        if (!a)
            continue;

        if (OverlapPoint(a, overlap_point)) {
            if (!first_hit)
                first_hit = a;
            if (!a->selected)
                return a;
        }
    }

    return first_hit;
}

AssetData* HitTestAssets(const Bounds2& hit_bounds) {
    AssetData* first_hit = nullptr;
    for (u32 i=GetAssetCount(); i>0; i--) {
        AssetData* a = GetAssetData(i-1);
        if (!a)
            continue;

        if (OverlapBounds(a, hit_bounds)) {
            if (!first_hit)
                first_hit = a;
            if (!a->selected)
                return a;
        }
    }

    return first_hit;
}

void DrawAsset(AssetData* a) {
    BindDepth(0.0f);
    if (a->vtable.draw)
        a->vtable.draw(a);
}

AssetData* GetFirstSelectedAsset() {
    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* a = GetAssetData(i);
        assert(a);
        if (a->selected)
            return a;
    }

    return nullptr;
}

void ClearAssetSelection() {
    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* a = GetAssetData(i);
        a->selected = false;
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

AssetData* GetAssetData(AssetType type, const Name* name) {
    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* a = GetAssetData(i);
        if ((type == ASSET_TYPE_UNKNOWN || a->type == type) && a->name == name)
            return a;
    }

    return nullptr;
}

void Clone(AssetData* dst, AssetData* src) {
    *(FatAssetData*)dst = *(FatAssetData*)src;

    if (dst->vtable.clone)
        dst->vtable.clone((AssetData*)dst);
}

AssetData* CreateAssetDataForImport(const std::filesystem::path& path) {
    AssetData* a = CreateAssetData(path);
    if (!a)
        return nullptr;

    LoadAssetMetadata(a, path);
    // LoadAssetData(a);
    // PostLoadAssetData(a);
    //SortAssets();

    return a;
}

void InitAssetData() {
    for (int i=0; i<g_editor.asset_path_count; i++) {
        std::vector<fs::path> asset_paths;
        GetFilesInDirectory(g_editor.asset_paths[i], asset_paths);

        for (auto& asset_path : asset_paths) {
            std::filesystem::path ext = asset_path.extension();
            if (ext == ".meta")
                continue;

            AssetData* a = nullptr;
            for (int asset_type=0; !a && asset_type<ASSET_TYPE_COUNT; asset_type++)
                a = CreateAssetData(asset_path);

            if (a)
                LoadAssetMetadata(a, asset_path);
        }
    }

    SortAssets();
}

void LoadAssetData(AssetData* a) {
    assert(a);

    if (a->loaded)
        return;

    a->loaded = true;

    if (a->vtable.load)
        a->vtable.load(a);
}

void PostLoadAssetData(AssetData* a) {
    assert(a);
    assert(a->loaded);

    if (a->post_loaded)
        return;

    if (a->vtable.post_load)
        a->vtable.post_load(a);

    a->post_loaded = true;
}

void LoadAssetData() {
    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        AssetData* a = GetAssetData(i);
        assert(a);
        LoadAssetData(a);
    }
}

void PostLoadAssetData() {
    for (u32 i=0, c=GetAssetCount(); i<c; i++) {
        PostLoadAssetData(GetAssetData(i));
    }
}

void HotloadEditorAsset(AssetType type, const Name* name){
    AssetData* a = GetAssetData(type, name);
    if (a != nullptr && a->vtable.reload)
        a->vtable.reload(a);
}

void MarkModified(AssetData* a) {
    a->modified = true;
}

void MarkMetaModified(AssetData* a) {
    a->meta_modified = true;
}

std::filesystem::path GetEditorAssetPath(const Name* name, const char* ext) {
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

void DeleteAsset(AssetData* a) {
    if (fs::exists(a->path))
        fs::remove(a->path);

    fs::path meta_path = fs::path(std::string(a->path) + ".meta");
    if (fs::exists(meta_path))
        fs::remove(meta_path);

    Free(a);
}

void SortAssets() {
    u32 asset_index = 0;
    for (u32 i=0; i<MAX_ASSETS; i++) {
        AssetData* a = GetAssetDataInternal(i);
        if (!a) continue;
        g_editor.assets[asset_index++] = i;
    }

    assert(asset_index == GetAssetCount());
}

fs::path GetTargetPath(AssetData* a) {
    std::string type_name_lower = ToString(a->importer->type);
    Lowercase(type_name_lower.data(), (u32)type_name_lower.size());
    fs::path source_relative_path = fs::relative(a->path, g_editor.asset_paths[a->asset_path_index]);
    fs::path target_short_path = type_name_lower / GetSafeFilename(source_relative_path.filename().string().c_str());
    fs::path target_path = g_editor.output_dir / target_short_path;
    target_path.replace_extension("");
    return target_path;
}

bool Rename(AssetData* a, const Name* new_name) {
    assert(a);
    assert(new_name);

    if (a->name == new_name)
        return true;

    fs::path new_path = fs::path(a->path).parent_path() / (std::string(new_name->value) + fs::path(a->path).extension().string());
    if (fs::exists(new_path))
        return false;

    fs::rename(a->path, new_path);
    Copy(a->path, sizeof(a->path), new_path.string().c_str());
    a->name = new_name;

    fs::path old_meta_path = fs::path(std::string(a->path) + ".meta");
    fs::path new_meta_path = fs::path(new_path.string() + ".meta");
    if (fs::exists(old_meta_path)) {
        fs::rename(old_meta_path, new_meta_path);
        return false;
    }

    return true;
}

AssetData* Duplicate(AssetData* a) {
    fs::path new_path = GetUniqueAssetPath(a->path);
    fs::copy(a->path, new_path);

    AssetData* d = (AssetData*)Alloc(g_editor.asset_allocator, sizeof(FatAssetData));
    Clone(d, a);
    Copy(d->path, sizeof(d->path), new_path.string().c_str());
    d->name = MakeCanonicalAssetName(new_path);
    d->selected = false;
    SortAssets();
    QueueImport(new_path);
    WaitForImportJobs();
    MarkModified(d);
    MarkMetaModified(d);
    return d;
}

std::filesystem::path GetUniqueAssetPath(const std::filesystem::path& path) {
    if (!fs::exists(path))
        return path;

    fs::path parent_path = path.parent_path();
    fs::path file_name = path.filename();
    fs::path ext = path.extension();
    file_name.replace_extension("");

    for (int i=0; ; i++) {
        fs::path new_path = parent_path / (file_name.string() + "_" + std::to_string(i) + ext.string());
        if (!fs::exists(new_path))
            return new_path;
    }
}

int GetSelectedAssets(AssetData** out_assets, int max_assets) {
    int selected_count = 0;
    for (u32 i=0, c=GetAssetCount(); i<c && selected_count < max_assets; i++) {
        AssetData* a = GetAssetData(i);
        assert(a);
        if (!a->selected) continue;
        out_assets[selected_count++] = a;
    }

    return selected_count;
}