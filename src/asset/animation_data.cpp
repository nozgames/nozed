//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

extern Asset* LoadAssetInternal(Allocator* allocator, const Name* asset_name, AssetSignature signature, AssetLoaderFunc loader, Stream* stream);
static void Init(AnimationData* a);

inline SkeletonData* GetSkeletonData(AnimationData* en) { return en->skeleton; }

void UpdateTransforms(AnimationData* en) {
    SkeletonData* es = GetSkeletonData(en);
    for (int bone_index=0; bone_index<es->bone_count; bone_index++) {
        BoneData& eb = es->bones[bone_index];
        Transform& frame = GetFrameTransform(en, bone_index, en->current_frame);

        en->animator.bones[bone_index] = TRS(
            eb.transform.position + frame.position,
            frame.rotation,
            eb.transform.scale);
    }

    for (int bone_index=1; bone_index<es->bone_count; bone_index++)
        en->animator.bones[bone_index] = en->animator.bones[es->bones[bone_index].parent_index] * en->animator.bones[bone_index];
}

void DrawEditorAnimationBone(AnimationData* en, int bone_index, const Vec2& position) {
    SkeletonData* es = GetSkeletonData(en);
    int parent_index = es->bones[bone_index].parent_index;
    if (parent_index < 0)
        parent_index = bone_index;

    Mat3 eb = en->animator.bones[bone_index] * Rotate(es->bones[bone_index].transform.rotation);
    Mat3 ep = en->animator.bones[parent_index];

    Vec2 p0 = TransformPoint(eb);
    Vec2 p1 = TransformPoint(eb, Vec2 {es->bones[bone_index].length, 0});
    Vec2 pp = TransformPoint(ep);
    DrawDashedLine(pp + position, p0 + position);
    DrawBone(p0 + position, p1 + position);
}

void DrawAnimationData(AssetData* ea) {
    assert(ea->type == ASSET_TYPE_ANIMATION);
    AnimationData* en = (AnimationData*)ea;
    SkeletonData* es = GetSkeletonData(en);

    BindColor(COLOR_WHITE);
    BindMaterial(g_view.shaded_material);
    for (int i=0; i<es->skinned_mesh_count; i++) {
        MeshData* skinned_mesh = es->skinned_meshes[i].mesh;
        if (!skinned_mesh || skinned_mesh->type != ASSET_TYPE_MESH)
            continue;

        DrawMesh(skinned_mesh, Translate(ea->position) * en->animator.bones[es->skinned_meshes[i].bone_index]);
    }

    for (int bone_index=0; bone_index<es->bone_count; bone_index++)
        DrawEditorAnimationBone(en, bone_index, ea->position);
}

static void ParseSkeletonBone(Tokenizer& tk, SkeletonData* es, int bone_index, int* bone_map) {
    if (!ExpectQuotedString(tk))
        throw std::exception("missing quoted bone name");

    bone_map[bone_index] = FindBoneIndex(es, GetName(tk));
}

void UpdateSkeleton(AnimationData* en)
{
    SkeletonData* es = GetSkeletonData(en);

    // Create a mapping table based on the name
    int bone_map[MAX_BONES];
    for (int i=0; i<MAX_BONES; i++)
        bone_map[i] = -1;

    for (int i=0; i<en->bone_count; i++)
    {
        int new_bone_index = FindBoneIndex(es, en->bones[i].name);
        if (new_bone_index == -1)
            continue;
        bone_map[new_bone_index] = i;
    }

    // recreate the frames using the new bone indicies and then fix the bones
    AnimationFrameData new_frames[MAX_ANIMATION_FRAMES];
    for (int frame_index=0; frame_index<en->frame_count; frame_index++)
        new_frames[frame_index] = en->frames[frame_index];

    // copy the new frames back
    memcpy(en->frames, new_frames, sizeof(new_frames));

    // fix the bones
    for (int i=0; i<es->bone_count; i++)
    {
        AnimationBoneData& enb = en->bones[i];
        enb.index = i;
        enb.name = es->bones[i].name;
    }

    en->bone_count = es->bone_count;

    UpdateBounds(en);
    UpdateTransforms(en);
}

static void ParseSkeleton(AnimationData* en, Tokenizer& tk, int* bone_map)
{
    if (!ExpectQuotedString(tk))
        throw std::exception("missing quoted skeleton name");

    en->skeleton_name = GetName(tk);

    SkeletonData* s = static_cast<SkeletonData*>(GetAssetData(ASSET_TYPE_SKELETON, en->skeleton_name));
    assert(s);

    if (!s->loaded)
        LoadAssetData(s);

    for (int i=0; i<s->bone_count; i++)
    {
        AnimationBoneData& enb = en->bones[i];
        BoneData& eb = s->bones[i];
        enb.name = eb.name;
        enb.index = i;
    }

    en->bone_count = s->bone_count;

    for (int frame_index=0; frame_index<MAX_ANIMATION_FRAMES; frame_index++)
        for (int bone_index=0; bone_index<MAX_BONES; bone_index++)
            SetIdentity(en->frames[frame_index].transforms[bone_index]);

    int bone_index = 0;
    while (!IsEOF(tk))
    {
        if (ExpectIdentifier(tk, "b"))
            ParseSkeletonBone(tk, s, bone_index++, bone_map);
        else
            break;
    }
}

static int ParseFrameBone(AnimationData* ea, Tokenizer& tk, int* bone_map)
{
    (void)ea;
    int bone_index;
    if (!ExpectInt(tk, &bone_index))
        ThrowError("expected bone index");

    return bone_map[bone_index];
}

static void ParseFramePosition(AnimationData* en, Tokenizer& tk, int bone_index, int frame_index)
{
    float x;
    if (!ExpectFloat(tk, &x))
        ThrowError("expected position 'x' value");
    float y;
    if (!ExpectFloat(tk, &y))
        ThrowError("expected position 'y' value");

    if (bone_index == -1)
        return;

    SetPosition(GetFrameTransform(en, bone_index, frame_index), {x,y});
}

static void ParseFrameHold(AnimationData* en, Tokenizer& tk, int frame_index)
{
    int hold;
    if (!ExpectInt(tk, &hold))
        ThrowError("expected hold value");

    en->frames[frame_index].hold = Max(0, hold);
}

static void ParseFrameRotation(AnimationData* en, Tokenizer& tk, int bone_index, int frame_index)
{
    float r;
    if (!ExpectFloat(tk, &r))
        ThrowError("expected rotation value");

    if (bone_index == -1)
        return;

    SetRotation(GetFrameTransform(en, bone_index, frame_index), r);
}

static void ParseFrameScale(AnimationData* en, Tokenizer& tk, int bone_index, int frame_index)
{
    float s;
    if (!ExpectFloat(tk, &s))
        ThrowError("expected scale value");

    if (bone_index == -1)
        return;

    SetScale(GetFrameTransform(en, bone_index, frame_index), s);
}

static void ParseFrame(AnimationData* en, Tokenizer& tk, int* bone_map)
{
    int bone_index = -1;
    en->frame_count++;
    while (!IsEOF(tk))
    {
        if (ExpectIdentifier(tk, "b"))
            bone_index = ParseFrameBone(en, tk, bone_map);
        else if (ExpectIdentifier(tk, "r"))
            ParseFrameRotation(en, tk, bone_index, en->frame_count - 1);
        else if (ExpectIdentifier(tk, "s"))
            ParseFrameScale(en, tk, bone_index, en->frame_count - 1);
        else if (ExpectIdentifier(tk, "p"))
            ParseFramePosition(en, tk, bone_index, en->frame_count - 1);
        else if (ExpectIdentifier(tk, "h"))
            ParseFrameHold(en, tk, en->frame_count - 1);
        else
            break;
    }
}

void UpdateBounds(AnimationData* en) {
    en->bounds = GetSkeletonData(en)->bounds;
}

static void PostLoadAnimationData(AssetData* ea) {
    assert(ea->type == ASSET_TYPE_ANIMATION);
    AnimationData* n = (AnimationData*)ea;

    n->skeleton = (SkeletonData*)GetAssetData(ASSET_TYPE_SKELETON, n->skeleton_name);
    if (!n->skeleton)
        return;

    PostLoadAssetData(n->skeleton);
    UpdateTransforms(n->skeleton);
    UpdateTransforms(n);
    UpdateBounds(n);
}

static void LoadAnimationData(AssetData* ea) {
    assert(ea);
    assert(ea->type == ASSET_TYPE_ANIMATION);
    AnimationData* en = (AnimationData*)ea;
    en->frame_count = 0;

    std::filesystem::path path = ea->path;
    std::string contents = ReadAllText(ALLOCATOR_DEFAULT, path);
    Tokenizer tk;
    Init(tk, contents.c_str());

    int bone_map[MAX_BONES];
    for (int i=0; i<MAX_BONES; i++)
        bone_map[i] = -1;

    while (!IsEOF(tk)) {
        if (ExpectIdentifier(tk, "s"))
            ParseSkeleton(en, tk, bone_map);
        else if (ExpectIdentifier(tk, "f"))
            ParseFrame(en, tk, bone_map);
        else
        {
            char error[1024];
            GetString(tk, error, sizeof(error) - 1);
            ThrowError("invalid token '%s' in animation", error);
        }
    }

    if (en->frame_count == 0) {
        AnimationFrameData& enf = en->frames[0];
        for (int i=0; i<MAX_BONES; i++)
            enf.transforms[i] = {
                .position = VEC2_ZERO,
                .scale = VEC2_ONE,
                .rotation = 0,
                .local_to_world = MAT3_IDENTITY,
                .world_to_local = MAT3_IDENTITY
            };
        en->frame_count = 1;
    }

    en->bounds = { VEC2_NEGATIVE_ONE, VEC2_ONE };
}

static AnimationData* LoadAnimationData(const std::filesystem::path& path) {
    std::string contents = ReadAllText(ALLOCATOR_DEFAULT, path);
    Tokenizer tk;
    Init(tk, contents.c_str());

    AnimationData* n = static_cast<AnimationData*>(CreateAssetData(path));
    assert(n);
    Init(n);
    LoadAssetData(n);
    MarkModified(n);
    return n;
}

void Serialize(AnimationData* en, Stream* output_stream, SkeletonData* es)
{
    assert(es);

    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE_ANIMATION;
    header.version = 1;
    WriteAssetHeader(output_stream, &header);

    int real_frame_count = en->frame_count;
    for (int frame_index=0; frame_index<en->frame_count; frame_index++)
        real_frame_count += en->frames[frame_index].hold;

    WriteU8(output_stream, (u8)es->bone_count);
    WriteU8(output_stream, (u8)real_frame_count);

    for (int i=0; i<es->bone_count; i++)
        WriteU8(output_stream, (u8)en->bones[i].index);

    for (int frame_index=0; frame_index<en->frame_count; frame_index++)
    {
        AnimationFrameData& enf = en->frames[frame_index];
        for (int hold_index=0; hold_index<enf.hold + 1; hold_index++)
        {
            for (int bone_index=0; bone_index<es->bone_count; bone_index++)
            {
                Transform& transform = enf.transforms[bone_index];
                BoneTransform bone_transform = {
                    .position = transform.position,
                    .scale = transform.scale,
                    .rotation = transform.rotation
                };

                WriteStruct(output_stream, bone_transform);
            }
        }
    }
}

Animation* ToAnimation(Allocator* allocator, AnimationData* en, const Name* name) {
    SkeletonData* es = GetSkeletonData(en);
    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 8192);
    if (!stream)
        return nullptr;

    Serialize(en, stream, es);
    SeekBegin(stream, 0);

    Animation* animation = (Animation*)LoadAssetInternal(allocator, name, ASSET_SIGNATURE_ANIMATION, LoadAnimation, stream);
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
        WriteCSTR(stream, "f\n");
        for (int bone_index=0; bone_index<es->bone_count; bone_index++) {
            Transform& bt = GetFrameTransform(en, bone_index, frame_index);

            bool has_pos = bt.position != VEC2_ZERO;
            bool has_rot = bt.rotation != 0.0f;

            if (!has_pos && !has_rot)
                continue;

            if (en->frames[frame_index].hold > 0)
                WriteCSTR(stream, "h %d", en->frames[frame_index].hold);

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

int InsertFrame(AnimationData* en, int frame_index) {
    int copy_frame = Max(0,frame_index - 1);

    SkeletonData* es = GetSkeletonData(en);
    for (int i=frame_index + 1; i<=en->frame_count; i++)
        en->frames[i] = en->frames[i - 1];

    en->frame_count++;

    if (copy_frame >= 0)
        for (int j=0; j<es->bone_count; j++)
            GetFrameTransform(en, j, frame_index) = GetFrameTransform(en, j, copy_frame);

    en->frames[frame_index].hold = 0;

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

Transform& GetFrameTransform(AnimationData* en, int bone_index, int frame_index) {
    assert(bone_index >= 0 && bone_index < MAX_BONES);
    assert(frame_index >= 0 && frame_index < en->frame_count);
    return en->frames[frame_index].transforms[bone_index];
}

int HitTestBone(AnimationData* en, const Vec2& world_pos) {
    SkeletonData* es = GetSkeletonData(en);
    UpdateTransforms(en);

    int best_bone_index = -1;
    float best_dist = F32_MAX;
    for (int bone_index=0; bone_index<es->bone_count; bone_index++) {
        BoneData* eb = &es->bones[bone_index];
        Mat3 collider_transform = Translate(GetAssetData()->position) * en->animator.bones[bone_index] * Scale(eb->length);
        if (!OverlapPoint(g_view.bone_collider, world_pos, collider_transform))
            continue;

        Mat3 local_to_world = en->animator.bones[bone_index] * Rotate(eb->transform.rotation);
        Vec2 b0 = TransformPoint(local_to_world);
        Vec2 b1 = TransformPoint(local_to_world, {eb->length, 0});
        float dist = DistanceFromLine(b0, b1, world_pos);
        if (dist < best_dist) {
            best_dist = dist;
            best_bone_index = bone_index;
        }
    }

    return best_bone_index;
}

AssetData* NewAnimationData(const std::filesystem::path& path) {
    (void)path;

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

static bool EditorAnimationOverlapPoint(AssetData* ea, const Vec2& position, const Vec2& overlap_point)
{
    assert(ea->type == ASSET_TYPE_ANIMATION);
    AnimationData* en = (AnimationData*)ea;
    return Contains(en->bounds + position, overlap_point);
}

static bool EditorAnimationOverlapBounds(AssetData* ea, const Bounds2& overlap_bounds)
{
    assert(ea->type == ASSET_TYPE_ANIMATION);
    AnimationData* en = (AnimationData*)ea;
    return Intersects(en->bounds + ea->position, overlap_bounds);
}

static void EditorAnimationClone(AssetData* ea)
{
    assert(ea->type == ASSET_TYPE_ANIMATION);
    AnimationData* en = (AnimationData*)ea;
    en->animation = nullptr;
    en->animator = {};
    UpdateTransforms(en);
    UpdateBounds(en);
}

static void EditorAnimationUndoRedo(AssetData* ea)
{
    assert(ea->type == ASSET_TYPE_ANIMATION);
    AnimationData* en = (AnimationData*)ea;
    UpdateSkeleton(en);
    UpdateTransforms(en);
}

static void Init(AnimationData* a) {
    extern void AnimationViewInit();
    extern void AnimationViewDraw();
    extern void AnimationViewUpdate();
    extern void AnimationViewShutdown();

    a->vtable = {
        .load = LoadAnimationData,
        .post_load = PostLoadAnimationData,
        .save = SaveAnimationData,
        .draw = DrawAnimationData,
        .overlap_point = EditorAnimationOverlapPoint,
        .overlap_bounds = EditorAnimationOverlapBounds,
        .clone = EditorAnimationClone,
        .undo_redo = EditorAnimationUndoRedo,
        .editor_begin = AnimationViewInit
    };
}

void InitAnimationData(AssetData* ea) {
    assert(ea);
    assert(ea->type == ASSET_TYPE_ANIMATION);
    AnimationData* en = (AnimationData*)ea;
    Init(en);
}
