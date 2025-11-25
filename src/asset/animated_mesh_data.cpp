//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

static void InitAnimatedMeshData(AnimatedMeshData* m);
extern void InitAnimatedMeshEditor(AnimatedMeshData* m);
extern void LoadMeshData(MeshData* m, Tokenizer& tk, bool multiple_mesh);
extern void SaveMeshData(MeshData* m, Stream* stream);

static void DrawAnimatedMeshData(AssetData* a) {
    assert(a->type == ASSET_TYPE_ANIMATED_MESH);
    AnimatedMeshData* m = static_cast<AnimatedMeshData*>(a);

    if (m->playing) {
        m->play_time = Update(m->playing, m->play_time);
        BindColor(COLOR_WHITE, GetActivePalette().color_offset_uv);
        BindMaterial(g_view.shaded_material);
        DrawMesh(m->playing, Translate(a->position), m->play_time);
    } else if (m->frame_count > 0) {
        DrawMesh(&m->frames[0], Translate(a->position));
    }
}

static void SaveAnimatedMeshData(AssetData* a, const std::filesystem::path& path) {
    assert(a->type == ASSET_TYPE_ANIMATED_MESH);
    AnimatedMeshData* m = static_cast<AnimatedMeshData*>(a);

    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);

    for (int i=0; i<m->frame_count; i++) {
        WriteCSTR(stream, "m\n");
        SaveMeshData(&m->frames[i], stream);
    }
    SaveStream(stream, path);
    Free(stream);
}

AnimatedMesh* ToAnimatedMesh(AnimatedMeshData* m) {
    Mesh* frames[ANIMATED_MESH_MAX_FRAMES];
    int frame_count = 0;
    for (int i=0; i<m->frame_count; i++) {
        Mesh* frame = ToMesh(&m->frames[i], true, false);
        if (!frame)
            continue;

        frames[frame_count++] = frame;
    }

    return CreateAnimatedMesh(ALLOCATOR_DEFAULT, m->name, frame_count, frames);
}

static void ParseMesh(AnimatedMeshData* m, Tokenizer& tk) {
    if (m->frame_count >= ANIMATED_MESH_MAX_FRAMES)
        ThrowError("too many frames in animated mesh");

    MeshData& frame = m->frames[m->frame_count++];

    InitMeshData(&frame);
    LoadMeshData(&frame, tk, true);
}

static void LoadAnimatedMeshData(AssetData* a) {
    assert(a);
    assert(a->type == ASSET_TYPE_ANIMATED_MESH);
    AnimatedMeshData* m = static_cast<AnimatedMeshData*>(a);

    std::string contents = ReadAllText(ALLOCATOR_DEFAULT, a->path);
    Tokenizer tk;
    Init(tk, contents.c_str());

    (void)m;
    while (!IsEOF(tk)) {
        if (ExpectIdentifier(tk, "m")) {
            ParseMesh(m, tk);
        } else {
            char error[1024];
            GetString(tk, error, sizeof(error) - 1);
            ThrowError("invalid token '%s' in mesh", error);
        }
    }

    Bounds2 bounds = m->frames->bounds;
    for (int i=1; i<m->frame_count; i++)
        bounds = Union(bounds, m->frames[i].bounds);

    a->bounds = bounds;
}

AnimatedMeshData* LoadAnimatedMeshData(const std::filesystem::path& path) {
    std::string contents = ReadAllText(ALLOCATOR_DEFAULT, path);
    Tokenizer tk;
    Init(tk, contents.c_str());

    AnimatedMeshData* m = static_cast<AnimatedMeshData*>(CreateAssetData(path));
    assert(m);
    LoadAnimatedMeshData(m);
    return m;
}

AssetData* NewAnimatedMeshData(const std::filesystem::path& path) {
    const char* default_mesh =
        "m\n"
        "v -1 -1 e 1 h 0\n"
        "v 1 -1 e 1 h 0\n"
        "v 1 1 e 1 h 0\n"
        "v -1 1 e 1 h 0\n"
        "f 0 1 2 3 c 1 0\n";

    std::string text = default_mesh;

    if (g_view.selected_asset_count == 1) {
        AssetData* selected = GetFirstSelectedAsset();
        if (selected && selected->type == ASSET_TYPE_MESH) {
            text = "m\n";
            text += ReadAllText(ALLOCATOR_DEFAULT, selected->path);
        }
    }

    std::filesystem::path full_path = path.is_relative() ?  std::filesystem::current_path() / "assets" / "animated_meshes" / path : path;
    full_path += ".amesh";

    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);
    WriteCSTR(stream, text.c_str());
    SaveStream(stream, full_path);
    Free(stream);

    return LoadAnimatedMeshData(full_path);
}

static void AllocateAnimatedMeshRuntimeData(AssetData* a) {
    assert(a->type == ASSET_TYPE_ANIMATED_MESH);
    AnimatedMeshData* n = static_cast<AnimatedMeshData*>(a);
    n->data = static_cast<RuntimeAnimatedMeshData*>(Alloc(ALLOCATOR_DEFAULT, sizeof(RuntimeAnimatedMeshData)));
    n->frames = n->data->frames;
}

static void CloneAnimatedMeshData(AssetData* a) {
    assert(a->type == ASSET_TYPE_ANIMATED_MESH);
    AnimatedMeshData* n = static_cast<AnimatedMeshData*>(a);
    RuntimeAnimatedMeshData* old_data = n->data;
    AllocateAnimatedMeshRuntimeData(n);
    memcpy(n->data, old_data, sizeof(RuntimeAnimatedMeshData));
}

static void DestroyAnimatedMeshData(AssetData* a) {
    AnimatedMeshData* d = static_cast<AnimatedMeshData*>(a);
    Free(d->data);
    d->data = nullptr;
}

static void PlayAnimatedMeshData(AssetData* a) {
    AnimatedMeshData* m = static_cast<AnimatedMeshData*>(a);
    assert(m);

    if (m->playing) {
        Free(m->playing);
        m->playing = nullptr;
    } else {
        m->playing = ToAnimatedMesh(m);
    }

    m->play_time = 0.0f;
}

static void InitAnimatedMeshData(AnimatedMeshData* m) {
    AllocateAnimatedMeshRuntimeData(m);

    m->vtable = {
        .destructor = DestroyAnimatedMeshData,
        .load = LoadAnimatedMeshData,
        .save = SaveAnimatedMeshData,
        .draw = DrawAnimatedMeshData,
        .play = PlayAnimatedMeshData,
        .clone = CloneAnimatedMeshData,
    };

    InitAnimatedMeshEditor(m);
}

void InitAnimatedMeshData(AssetData* a) {
    assert(a);
    assert(a->type == ASSET_TYPE_ANIMATED_MESH);
    InitAnimatedMeshData(static_cast<AnimatedMeshData*>(a));
}
