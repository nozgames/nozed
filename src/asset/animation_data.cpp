//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

extern Asset* LoadAssetInternal(Allocator* allocator, const Name* asset_name, AssetType asset_type, AssetLoaderFunc loader, Stream* stream);
static void InitAnimationData(AnimationData* a);
extern void InitAnimationEditor(AnimationData* a);

inline SkeletonData* GetSkeletonData(AnimationData* en) { return en->skeleton; }

void UpdateTransforms(AnimationData* n) {
    SkeletonData* s = GetSkeletonData(n);
    for (int bone_index=0; bone_index<s->bone_count; bone_index++) {
        BoneData* b = &s->bones[bone_index];
        Transform& frame = GetFrameTransform(n, bone_index, n->current_frame);

        n->animator->bones[bone_index] = TRS(
            b->transform.position + frame.position,
            b->transform.rotation + frame.rotation,
            b->transform.scale);
    }

    for (int bone_index=1; bone_index<s->bone_count; bone_index++)
        n->animator->bones[bone_index] =
            n->animator->bones[s->bones[bone_index].parent_index] * n->animator->bones[bone_index];
}

void DrawEditorAnimationBone(AnimationData* n, int bone_index, const Vec2& position) {
    SkeletonData* s = GetSkeletonData(n);
    int parent_index = s->bones[bone_index].parent_index;
    if (parent_index < 0)
        parent_index = bone_index;

    Mat3 bt = n->animator->bones[bone_index];
    Mat3 pt = n->animator->bones[parent_index];

    Vec2 p0 = TransformPoint(bt);
    Vec2 p1 = TransformPoint(bt, Vec2 {s->bones[bone_index].length, 0});
    Vec2 pp = TransformPoint(pt);
    DrawDashedLine(pp + position, p0 + position);
    DrawBone(p0 + position, p1 + position);
}

void DrawAnimationData(AssetData* a) {
    assert(a->type == ASSET_TYPE_ANIMATION);
    AnimationData* n = static_cast<AnimationData*>(a);
    SkeletonData* s = GetSkeletonData(n);

    BindColor(COLOR_WHITE);
    BindSkeleton(&s->bones[0].world_to_local, sizeof(BoneData), n->animator->bones, 0, s->bone_count);
    for (int i=0; i<s->skin_count; i++) {
        MeshData* skinned_mesh = s->skins[i].mesh;
        if (!skinned_mesh || skinned_mesh->type != ASSET_TYPE_MESH)
            continue;

        DrawMesh(skinned_mesh, Translate(a->position), g_view.shaded_skinned_material);
    }
}

static void ParseSkeletonBone(Tokenizer& tk, SkeletonData* es, int bone_index, int* bone_map) {
    if (!ExpectQuotedString(tk))
        throw std::exception("missing quoted bone name");

    bone_map[bone_index] = FindBoneIndex(es, GetName(tk));
}

void UpdateSkeleton(AnimationData* n) {
    SkeletonData* s = GetSkeletonData(n);

    int bone_map[MAX_BONES];
    for (int i=0; i<MAX_BONES; i++)
        bone_map[i] = -1;

    for (int i=0; i<n->bone_count; i++) {
        int new_bone_index = FindBoneIndex(s, n->bones[i].name);
        if (new_bone_index == -1)
            continue;
        bone_map[new_bone_index] = i;
    }

    AnimationFrameData new_frames[MAX_ANIMATION_FRAMES];
    for (int frame_index=0; frame_index<n->frame_count; frame_index++)
        new_frames[frame_index] = n->frames[frame_index];

    memcpy(n->frames, new_frames, sizeof(new_frames));

    for (int i=0; i<s->bone_count; i++) {
        AnimationBoneData& ab = n->bones[i];
        ab.index = i;
        ab.name = s->bones[i].name;
    }

    n->bone_count = s->bone_count;

    UpdateBounds(n);
    UpdateTransforms(n);
}

static void ParseSkeleton(AnimationData* n, Tokenizer& tk, int* bone_map) {
    if (!ExpectQuotedString(tk))
        throw std::exception("missing quoted skeleton name");

    n->skeleton_name = GetName(tk);

    SkeletonData* s = static_cast<SkeletonData*>(GetAssetData(ASSET_TYPE_SKELETON, n->skeleton_name));
    assert(s);

    if (!s->loaded)
        LoadAssetData(s);

    for (int i=0; i<s->bone_count; i++)
    {
        AnimationBoneData& enb = n->bones[i];
        BoneData& eb = s->bones[i];
        enb.name = eb.name;
        enb.index = i;
    }

    n->bone_count = s->bone_count;

    for (int frame_index=0; frame_index<MAX_ANIMATION_FRAMES; frame_index++)
        for (int bone_index=0; bone_index<MAX_BONES; bone_index++)
            SetIdentity(n->frames[frame_index].transforms[bone_index]);

    int bone_index = 0;
    while (!IsEOF(tk))
    {
        if (ExpectIdentifier(tk, "b"))
            ParseSkeletonBone(tk, s, bone_index++, bone_map);
        else
            break;
    }
}

static int ParseFrameBone(AnimationData* a, Tokenizer& tk, int* bone_map) {
    (void)a;
    int bone_index;
    if (!ExpectInt(tk, &bone_index))
        ThrowError("expected bone index");

    return bone_map[bone_index];
}

static void ParseFramePosition(AnimationData* n, Tokenizer& tk, int bone_index, int frame_index) {
    float x;
    if (!ExpectFloat(tk, &x))
        ThrowError("expected position 'x' value");
    float y;
    if (!ExpectFloat(tk, &y))
        ThrowError("expected position 'y' value");

    if (bone_index == -1)
        return;

    SetPosition(GetFrameTransform(n, bone_index, frame_index), {x,y});
}

static void ParseFrameHold(AnimationData* n, Tokenizer& tk, int frame_index) {
    int hold;
    if (!ExpectInt(tk, &hold))
        ThrowError("expected hold value");

    n->frames[frame_index].hold = Max(0, hold);
}

static void ParseFrameRotation(AnimationData* n, Tokenizer& tk, int bone_index, int frame_index) {
    float r;
    if (!ExpectFloat(tk, &r))
        ThrowError("expected rotation value");

    if (bone_index == -1)
        return;

    SetRotation(GetFrameTransform(n, bone_index, frame_index), r);
}

static void ParseFrameScale(AnimationData* n, Tokenizer& tk, int bone_index, int frame_index) {
    float s;
    if (!ExpectFloat(tk, &s))
        ThrowError("expected scale value");

    if (bone_index == -1)
        return;

    SetScale(GetFrameTransform(n, bone_index, frame_index), s);
}

static void ParseFrame(AnimationData* n, Tokenizer& tk, int* bone_map) {
    int bone_index = -1;
    n->frame_count++;
    while (!IsEOF(tk)) {
        if (ExpectIdentifier(tk, "b"))
            bone_index = ParseFrameBone(n, tk, bone_map);
        else if (ExpectIdentifier(tk, "r"))
            ParseFrameRotation(n, tk, bone_index, n->frame_count - 1);
        else if (ExpectIdentifier(tk, "s"))
            ParseFrameScale(n, tk, bone_index, n->frame_count - 1);
        else if (ExpectIdentifier(tk, "p"))
            ParseFramePosition(n, tk, bone_index, n->frame_count - 1);
        else if (ExpectIdentifier(tk, "h"))
            ParseFrameHold(n, tk, n->frame_count - 1);
        else
            break;
    }
}

void UpdateBounds(AnimationData* n) {
    n->bounds = GetSkeletonData(n)->bounds;
}

static void PostLoadAnimationData(AssetData* a) {
    assert(a->type == ASSET_TYPE_ANIMATION);
    AnimationData* n = static_cast<AnimationData*>(a);

    n->skeleton = static_cast<SkeletonData*>(GetAssetData(ASSET_TYPE_SKELETON, n->skeleton_name));
    if (!n->skeleton)
        return;

    PostLoadAssetData(n->skeleton);
    UpdateTransforms(n->skeleton);
    UpdateTransforms(n);
    UpdateBounds(n);
}

static void LoadAnimationData(AssetData* a) {
    assert(a);
    assert(a->type == ASSET_TYPE_ANIMATION);
    AnimationData* n = static_cast<AnimationData*>(a);
    n->frame_count = 0;

    std::filesystem::path path = a->path;
    std::string contents = ReadAllText(ALLOCATOR_DEFAULT, path);
    Tokenizer tk;
    Init(tk, contents.c_str());

    int bone_map[MAX_BONES];
    for (int i=0; i<MAX_BONES; i++)
        bone_map[i] = -1;

    while (!IsEOF(tk)) {
        if (ExpectIdentifier(tk, "s"))
            ParseSkeleton(n, tk, bone_map);
        else if (ExpectIdentifier(tk, "f"))
            ParseFrame(n, tk, bone_map);
        else {
            char error[1024];
            GetString(tk, error, sizeof(error) - 1);
            ThrowError("invalid token '%s' in animation", error);
        }
    }

    if (n->frame_count == 0) {
        AnimationFrameData& enf = n->frames[0];
        for (int i=0; i<MAX_BONES; i++)
            enf.transforms[i] = {
                .position = VEC2_ZERO,
                .scale = VEC2_ONE,
                .rotation = 0,
                .local_to_world = MAT3_IDENTITY,
                .world_to_local = MAT3_IDENTITY
            };
        n->frame_count = 1;
    }

    n->bounds = { VEC2_NEGATIVE_ONE, VEC2_ONE };
}

static AnimationData* LoadAnimationData(const std::filesystem::path& path) {
    std::string contents = ReadAllText(ALLOCATOR_DEFAULT, path);
    Tokenizer tk;
    Init(tk, contents.c_str());

    AnimationData* n = static_cast<AnimationData*>(CreateAssetData(path));
    assert(n);
    InitAnimationData(n);
    LoadAssetData(n);
    MarkModified(n);
    return n;
}

static void SerializeTransform(Stream* stream, const Transform& transform) {
    BoneTransform bone_transform = {
        .position = transform.position,
        .rotation = transform.rotation,
        .scale = transform.scale
    };
    WriteStruct(stream, bone_transform);
}

void Serialize(AnimationData* n, Stream* output_stream, SkeletonData* s) {
    assert(s);

    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE;
    header.type = ASSET_TYPE_ANIMATION;
    header.version = 1;
    WriteAssetHeader(output_stream, &header);

    bool looping = (n->flags & ANIMATION_FLAG_LOOPING) != 0;

    int real_frame_count = n->frame_count;
    for (int frame_index=0; frame_index<n->frame_count; frame_index++)
        real_frame_count += n->frames[frame_index].hold;

    if (looping && n->frame_count > 1) {
        real_frame_count--;
        real_frame_count -= n->frames[n->frame_count - 1].hold;
    }

    WriteU8(output_stream, (u8)s->bone_count);
    WriteU8(output_stream, (u8)real_frame_count);
    WriteU8(output_stream, (u8)g_config->GetInt("animation", "frame_rate", ANIMATION_FRAME_RATE));
    WriteU8(output_stream, (u8)n->flags);

    for (int i=0; i<s->bone_count; i++)
        WriteU8(output_stream, (u8)n->bones[i].index);

    for (int frame_index=0; frame_index<n->frame_count; frame_index++) {
        AnimationFrameData& f = n->frames[frame_index];
        for (int bone_index=0; bone_index<s->bone_count; bone_index++)
            SerializeTransform(output_stream, f.transforms[bone_index]);

        if (looping && frame_index == n->frame_count - 1)
            continue;

        if (f.hold == 0)
            continue;

        int next_frame_index = looping
            ? (frame_index + 1) % n->frame_count
            : Min(frame_index + 1, n->frame_count - 1);

        AnimationFrameData& nf = n->frames[next_frame_index];
        for (int i=0; i<f.hold; i++) {
            for (int bone_index=0; bone_index<s->bone_count; bone_index++) {
                const Transform& t = f.transforms[bone_index];
                const Transform& nt = nf.transforms[bone_index];
                SerializeTransform(output_stream, Mix(t, nt, (float)(i + 1) / (float)(f.hold + 1)));
            }
        }
    }

    // Write one more frame at the end for non-looping animations
    if (!looping || n->frame_count == 1) {
        AnimationFrameData& f = n->frames[n->frame_count-1];
        for (int bone_index=0; bone_index<s->bone_count; bone_index++)
            SerializeTransform(output_stream, f.transforms[bone_index]);
    }
}

Animation* ToAnimation(Allocator* allocator, AnimationData* n) {
    SkeletonData* es = GetSkeletonData(n);
    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 8192);
    if (!stream)
        return nullptr;

    Serialize(n, stream, es);
    SeekBegin(stream, 0);

    Animation* animation = static_cast<Animation*>(LoadAssetInternal(allocator, n->name, ASSET_TYPE_ANIMATION, LoadAnimation, stream));
    Free(stream);

    return animation;
}

static void SaveAnimationData(AssetData* ea, const std::filesystem::path& path) {
    assert(ea->type == ASSET_TYPE_ANIMATION);
    AnimationData* en = (AnimationData*)ea;
    SkeletonData* es = GetSkeletonData(en);

    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);

    WriteCSTR(stream, "s \"%s\"\n", en->skeleton_name->value);

    for (int i=0; i<es->bone_count; i++) {
        const AnimationBoneData& eab = en->bones[i];
        WriteCSTR(stream, "b \"%s\"\n", eab.name->value);
    }

    for (int frame_index=0; frame_index<en->frame_count; frame_index++) {
        WriteCSTR(stream, "f");
        if (en->frames[frame_index].hold > 0)
            WriteCSTR(stream, " h %d", en->frames[frame_index].hold);
        WriteCSTR(stream, "\n");

        for (int bone_index=0; bone_index<es->bone_count; bone_index++) {
            Transform& bt = GetFrameTransform(en, bone_index, frame_index);

            bool has_pos = bt.position != VEC2_ZERO;
            bool has_rot = bt.rotation != 0.0f;

            if (!has_pos && !has_rot)
                continue;

            WriteCSTR(stream, "b %d", bone_index);

            if (has_pos)
                WriteCSTR(stream, " p %f %f", bt.position.x, bt.position.y);

            if (has_rot)
                WriteCSTR(stream, " r %f", bt.rotation);

            WriteCSTR(stream, "\n");
        }
    }

    SaveStream(stream, path);
    Free(stream);
}

int InsertFrame(AnimationData* n, int frame_index) {
    int copy_frame = Max(0,frame_index - 1);

    SkeletonData* s = GetSkeletonData(n);
    for (int i=frame_index + 1; i<=n->frame_count; i++)
        n->frames[i] = n->frames[i - 1];

    n->frame_count++;

    if (copy_frame >= 0)
        for (int j=0; j<s->bone_count; j++)
            GetFrameTransform(n, j, frame_index) = GetFrameTransform(n, j, copy_frame);

    n->frames[frame_index].hold = 0;

    return frame_index;
}

int DeleteFrame(AnimationData* en, int frame_index) {
    if (en->frame_count <= 1)
        return frame_index;

    for (int i=frame_index; i<en->frame_count - 1; i++)
        en->frames[i] = en->frames[i + 1];

    en->frame_count--;

    return Min(frame_index, en->frame_count - 1);
}

Transform& GetFrameTransform(AnimationData* n, int bone_index, int frame_index) {
    assert(bone_index >= 0 && bone_index < MAX_BONES);
    assert(frame_index >= 0 && frame_index < n->frame_count);
    return n->frames[frame_index].transforms[bone_index];
}

AssetData* NewAnimationData(const std::filesystem::path& path) {
    if (g_view.selected_asset_count != 1) {
        LogError("no skeleton selected");
        return nullptr;
    }

    std::filesystem::path full_path = path.is_relative() ?  std::filesystem::current_path() / "assets" / "animations" / path : path;
    full_path += ".anim";

    AssetData* skeleton_asset = GetFirstSelectedAsset();
    if (!skeleton_asset || skeleton_asset->type != ASSET_TYPE_SKELETON)
    {
        LogError("no skeleton selected");
        return nullptr;
    }

    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);
    WriteCSTR(stream, "s \"%s\"\n", skeleton_asset->name->value);
    SaveStream(stream, full_path);
    Free(stream);

    QueueImport(full_path);
    WaitForImportJobs();
    return LoadAnimationData(full_path);
}

static void HandleAnimationUndoRedo(AssetData* a) {
    assert(a->type == ASSET_TYPE_ANIMATION);
    AnimationData* n = static_cast<AnimationData*>(a);
    UpdateSkeleton(n);
    UpdateTransforms(n);
}

static void LoadAnimationMetadata(AssetData* a, Props* meta) {
    AnimationData* n = static_cast<AnimationData*>(a);
    n->flags = ANIMATION_FLAG_NONE;
    if (meta->GetBool("animation", "loop", true))
        n->flags |= ANIMATION_FLAG_LOOPING;
    if (meta->GetBool("animation", "root_motion", false))
        n->flags |= ANIMATION_FLAG_ROOT_MOTION;
}

static void AllocateAnimationRuntimeData(AssetData* a) {
    assert(a->type == ASSET_TYPE_ANIMATION);
    AnimationData* n = static_cast<AnimationData*>(a);
    n->data = static_cast<RuntimeAnimationData*>(Alloc(ALLOCATOR_DEFAULT, sizeof(RuntimeAnimationData)));
    n->bones = n->data->bones;
    n->frames = n->data->frames;
    n->animator = &n->data->animator;
}

static void CloneAnimationData(AssetData* a) {
    assert(a->type == ASSET_TYPE_ANIMATION);
    AnimationData* n = static_cast<AnimationData*>(a);
    RuntimeAnimationData* old_data = n->data;
    AllocateAnimationRuntimeData(n);
    memcpy(n->data, old_data, sizeof(RuntimeAnimationData));
    n->animation = nullptr;
    *n->animator = {};
    UpdateTransforms(n);
    UpdateBounds(n);
}

static void DestroyAnimationData(AssetData* a) {
    AnimationData* d = static_cast<AnimationData*>(a);
    Free(d->data);
    d->data = nullptr;
}

static void InitAnimationData(AnimationData* a) {
    AllocateAnimationRuntimeData(a);

    a->vtable = {
        .destructor = DestroyAnimationData,
        .load = LoadAnimationData,
        .post_load = PostLoadAnimationData,
        .save = SaveAnimationData,
        .load_metadata = LoadAnimationMetadata,
        .draw = DrawAnimationData,
        .clone = CloneAnimationData,
        .undo_redo = HandleAnimationUndoRedo
    };

    InitAnimationEditor(a);
}

void InitAnimationData(AssetData* a) {
    assert(a);
    assert(a->type == ASSET_TYPE_ANIMATION);
    InitAnimationData(static_cast<AnimationData*>(a));
}
