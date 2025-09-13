//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

extern Asset* LoadAssetInternal(Allocator* allocator, const Name* asset_name, AssetSignature signature, AssetLoaderFunc loader, Stream* stream);
extern EditorAsset* CreateEditorAsset(const std::filesystem::path& path, EditorAssetType type);
static EditorSkeleton& GetEditorSkeleton(EditorAnimation& en) { return *en.skeleton_asset->skeleton; }
static EditorBone& GetEditorBone(EditorAnimation& en, int bone_index) { return en.skeleton_asset->skeleton->bones[bone_index]; }

void DrawEditorAnimation(EditorAsset& ea)
{
    EditorAnimation& en = *ea.anim;
    BindMaterial(g_view.vertex_material);
    BindColor(COLOR_BLACK);

    EditorSkeleton& es = GetEditorSkeleton(en);

    BindColor(COLOR_WHITE);
    BindMaterial(g_view.material);
    for (int i=0; i<es.skinned_mesh_count; i++)
    {
        const EditorAsset& skinned_mesh_asset = *g_view.assets[es.skinned_meshes[i].asset_index];
        if (skinned_mesh_asset.type != EDITOR_ASSET_TYPE_MESH)
            continue;

        BindTransform(TRS(ea.position, 0, VEC2_ONE) * GetLocalToWorld(GetFrameTransform(en, es.skinned_meshes[i].bone_index, en.current_frame)));
        DrawMesh(ToMesh(*skinned_mesh_asset.mesh));
    }

    for (int bone_index=1; bone_index<en.bone_count; bone_index++)
    {
        Transform& t0 = GetFrameTransform(en, es.bones[bone_index].parent_index, en.current_frame);
        Transform& t1 = GetFrameTransform(en, bone_index, en.current_frame);
        Vec2 b1 = TransformPoint(t0);
        Vec2 b2 = TransformPoint(t1);
        DrawBone(b1 + ea.position, b2 + ea.position);
    }
}

static void ParseSkeletonBone(Tokenizer& tk, const EditorSkeleton& es, int bone_index, int* bone_map)
{
    if (!ExpectQuotedString(tk))
        throw std::exception("missing quoted bone name");

    bone_map[bone_index] = FindBoneIndex(es, GetName(tk));
}

static void ParseSkeleton(EditorAnimation& en, Tokenizer& tk, int* bone_map)
{
    if (!ExpectQuotedString(tk))
        throw std::exception("missing quoted skeleton name");

    en.skeleton_name = GetName(tk);

    std::filesystem::path skeleton_path = GetEditorAssetPath(en.skeleton_name, ".skel");
    EditorSkeleton* es = LoadEditorSkeleton(ALLOCATOR_DEFAULT, skeleton_path);

    en.bone_count = es->bone_count;
    for (int i=0; i<es->bone_count; i++)
    {
        EditorAnimationBone& enb = en.bones[i];
        EditorBone& eb = es->bones[i];
        enb.name = eb.name;
        enb.index = i;
    }

    for (int frame_index=0; frame_index<MAX_ANIMATION_FRAMES; frame_index++)
        for (int bone_index=0; bone_index<en.bone_count; bone_index++)
            SetIdentity(en.frames[frame_index * MAX_BONES + bone_index]);

    int bone_index = 0;
    while (!IsEOF(tk))
    {
        if (ExpectIdentifier(tk, "b"))
            ParseSkeletonBone(tk, *es, bone_index++, bone_map);
        else
            break;
    }
}

static int ParseFrameBone(EditorAnimation& ea, Tokenizer& tk, int* bone_map)
{
    (void)ea;
    int bone_index;
    if (!ExpectInt(tk, &bone_index))
        ThrowError("expected bone index");

    return bone_map[bone_index];
}

static void ParseFramePosition(EditorAnimation& en, Tokenizer& tk, int bone_index, int frame_index)
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

static void ParseFrameRotation(EditorAnimation& en, Tokenizer& tk, int bone_index, int frame_index)
{
    float r;
    if (!ExpectFloat(tk, &r))
        ThrowError("expected rotation value");

    if (bone_index == -1)
        return;

    SetRotation(GetFrameTransform(en, bone_index, frame_index), r);
}

static void ParseFrameScale(EditorAnimation& en, Tokenizer& tk, int bone_index, int frame_index)
{
    float s;
    if (!ExpectFloat(tk, &s))
        ThrowError("expected scale value");

    if (bone_index == -1)
        return;

    SetScale(GetFrameTransform(en, bone_index, frame_index), s);
}

static void ParseFrame(EditorAnimation& en, Tokenizer& tk, int* bone_map)
{
    int bone_index = -1;
    en.frame_count++;
    while (!IsEOF(tk))
    {
        if (ExpectIdentifier(tk, "b"))
            bone_index = ParseFrameBone(en, tk, bone_map);
        else if (ExpectIdentifier(tk, "r"))
            ParseFrameRotation(en, tk, bone_index, en.frame_count - 1);
        else if (ExpectIdentifier(tk, "s"))
            ParseFrameScale(en, tk, bone_index, en.frame_count - 1);
        else if (ExpectIdentifier(tk, "p"))
            ParseFramePosition(en, tk, bone_index, en.frame_count - 1);
        else
            break;
    }
}

static void PostLoadEditorAnimation(EditorAsset& ea)
{
    ea.anim->skeleton_asset = GetEditorAsset(FindEditorAssetByName(ea.anim->skeleton_name));
}

EditorAnimation* LoadEditorAnimation(Allocator* allocator, const std::filesystem::path& path)
{
    std::string contents = ReadAllText(ALLOCATOR_DEFAULT, path);
    Tokenizer tk;
    Init(tk, contents.c_str());

    EditorAnimation* en = (EditorAnimation*)Alloc(allocator, sizeof(EditorAnimation));
    int bone_map[MAX_BONES];
    for (int i=0; i<MAX_BONES; i++)
        bone_map[i] = -1;

    try
    {
        while (!IsEOF(tk))
        {
            if (ExpectIdentifier(tk, "s"))
                ParseSkeleton(*en, tk, bone_map);
            if (ExpectIdentifier(tk, "f"))
                ParseFrame(*en, tk, bone_map);
            else
                ThrowError("invalid token '%s' in animation", GetString(tk));
        }
    }
    catch (std::exception&)
    {
        Free(en);
        return nullptr;
    }

    en->bounds = { VEC2_NEGATIVE_ONE, VEC2_ONE };

    return en;
}

void UpdateBounds(EditorAnimation& en)
{
    if (!en.skeleton_asset)
        return;

    en.bounds = en.skeleton_asset->skeleton->bounds;
}

void Serialize(EditorAnimation& en, Stream* output_stream)
{
    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE_ANIMATION;
    header.version = 1;
    WriteAssetHeader(output_stream, &header);

    EditorSkeleton* es = LoadEditorSkeleton(ALLOCATOR_DEFAULT, std::string(en.skeleton_name->value) + ".skel");
    if (!es)
        throw std::runtime_error("invalid skeleton");

    WriteU8(output_stream, (u8)en.bone_count);
    WriteU8(output_stream, (u8)en.frame_count);

    // todo: we could remove bones that have no actual data?
    for (int i=0; i<en.bone_count; i++)
        WriteU8(output_stream, (u8)en.bones[i].index);

    // Write all bone transforms
    for (int frame_index=0; frame_index<en.frame_count; frame_index++)
        for (int bone_index=0; bone_index<en.bone_count; bone_index++)
        {
            Transform& transform = en.frames[frame_index * MAX_BONES + bone_index];
            BoneTransform bone_transform = {
                .position = transform.position,
                .scale = transform.scale,
                .rotation = transform.rotation
            };
            WriteStruct(output_stream, bone_transform);
        }
}

Animation* ToAnimation(Allocator* allocator, EditorAnimation& en, const Name* name)
{
    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 8192);
    if (!stream)
        return nullptr;
    Serialize(en, stream);
    SeekBegin(stream, 0);

    Animation* animation = (Animation*)LoadAssetInternal(allocator, name, ASSET_SIGNATURE_ANIMATION, LoadAnimation, stream);
    Free(stream);

    return animation;
}

void SaveEditorAnimation(EditorAnimation& en, const std::filesystem::path& path)
{
    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);

    WriteCSTR(stream, "s \"%s\"\n", en.skeleton_name->value);

    for (int i=0; i<en.bone_count; i++)
    {
        const EditorAnimationBone& eab = en.bones[i];
        WriteCSTR(stream, "b \"%s\"\n", eab.name->value);
    }

    for (int frame_index=0; frame_index<en.frame_count; frame_index++)
    {
        WriteCSTR(stream, "f\n");
        for (int bone_index=0; bone_index<en.bone_count; bone_index++)
        {
            Transform& bt = GetFrameTransform(en, bone_index, frame_index);

            bool has_pos = bt.position != VEC2_ZERO;
            bool has_rot = bt.rotation != 0.0f;
//            bool has_scale = bt.scale != 1.0f;

            if (!has_pos && !has_rot)
                continue;

            WriteCSTR(stream, "b %d", bone_index);

            if (has_pos)
                WriteCSTR(stream, " p %g %g", bt.position.x, bt.position.y);

            if (has_rot)
                WriteCSTR(stream, " r %g", bt.rotation);

            // if (has_scale)
            //     WriteCSTR(stream, " s %g", bt.scale);

            WriteCSTR(stream, "\n");
        }
    }

    SaveStream(stream, path);
    Free(stream);
}

int InsertFrame(EditorAnimation& en, int frame_index)
{
    int copy_frame = 1;
    if (frame_index > 0)
        copy_frame = frame_index - 1;

    for (int i=frame_index + 1; i<en.frame_count; i++)
        for (int j=0; j<en.bone_count; j++)
            GetFrameTransform(en, j, i) = GetFrameTransform(en, j, i - 1);

    if (copy_frame > 0)
        for (int j=0; j<en.bone_count; j++)
            GetFrameTransform(en, j, frame_index) = GetFrameTransform(en, j, copy_frame);

    en.frame_count++;

    return frame_index;
}

int DeleteFrame(EditorAnimation& en, int frame_index)
{
    if (en.frame_count <= 1)
        return frame_index;

    for (int i=frame_index; i<en.frame_count - 1; i++)
        for (int j=0; j<en.bone_count; j++)
            GetFrameTransform(en, j, i) = GetFrameTransform(en, j, i + 1);

    en.frame_count--;

    return Min(frame_index, en.frame_count - 1);
}

Transform& GetFrameTransform(EditorAnimation& en, int bone_index, int frame_index)
{
    assert(bone_index >= 0 && bone_index < en.bone_count);
    assert(frame_index >= 0 && frame_index < en.frame_count);
    return en.frames[frame_index * MAX_BONES + bone_index];
}

bool HitTestBone(EditorAnimation& en, const Vec2& world_pos)
{
    const float size = g_view.select_size;
    float best_dist = F32_MAX;
    int best_bone_index = -1;
    for (int bone_index=0; bone_index<en.bone_count; bone_index++)
    {
        Vec2 b0 = TransformPoint(GetFrameTransform(en, bone_index, en.current_frame));
        float dist = Length(b0 - world_pos);
        if (dist < size && dist < best_dist)
        {
            best_dist = dist;
            best_bone_index = bone_index;
        }
    }

    if (best_bone_index != -1)
        return best_bone_index;

    best_bone_index = -1;
    best_dist = F32_MAX;
    for (int bone_index=1; bone_index<en.bone_count; bone_index++)
    {
        Vec2 b0 = TransformPoint(GetFrameTransform(en, bone_index, en.current_frame));
        Vec2 b1 = TransformPoint(GetFrameTransform(en, GetEditorBone(en, bone_index).parent_index, en.current_frame));
        float dist = DistanceFromLine(b0, b1, world_pos);
        if (dist < size && dist < best_dist)
        {
            best_dist = dist;
            best_bone_index = bone_index;
        }
    }

    return best_bone_index;
}

EditorAsset* LoadEditorAnimationAsset(const std::filesystem::path& path)
{
    EditorAnimation* en = LoadEditorAnimation(ALLOCATOR_DEFAULT, path);
    if (!en)
        return nullptr;

    EditorAsset* ea = CreateEditorAsset(path, EDITOR_ASSET_TYPE_ANIMATION);
    ea->anim = en;
    ea->vtable = {
        .post_load = PostLoadEditorAnimation
    };
    return ea;
}