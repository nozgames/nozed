//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

static void Init(SkeletonData* s);
extern void InitSkeletonEditor(SkeletonData* s);

extern Asset* LoadAssetInternal(Allocator* allocator, const Name* asset_name, AssetSignature signature, AssetLoaderFunc loader, Stream* stream);

void DrawEditorSkeletonBone(SkeletonData* s, int bone_index, const Vec2& position) {
    BoneData* eb = s->bones + bone_index;
    Mat3 local_to_world = eb->local_to_world * Rotate(eb->transform.rotation);
    DrawBone(
        local_to_world,
        GetParentLocalToWorld(s, eb, local_to_world),
        position,
        eb->length);
}

static void SortSkin(SkeletonData* s) {
    qsort(s->skinned_meshes, s->skinned_mesh_count, sizeof(SkinnedMesh),
        [](void const* p, void const* arg) {
            SkinnedMesh* a = (SkinnedMesh*)p;
            SkinnedMesh* b = (SkinnedMesh*)arg;
            return a->mesh->sort_order - b->mesh->sort_order;
        });
}

void DrawEditorSkeleton(SkeletonData* s, const Vec2& position, bool) {
    if (g_view.draw_mode != VIEW_DRAW_MODE_WIREFRAME) {
        BindColor(COLOR_WHITE);
        for (int i=0; i<s->skinned_mesh_count; i++) {
            BoneData& bone = s->bones[s->skinned_meshes[i].bone_index];
            MeshData* skinned_mesh = s->skinned_meshes[i].mesh;
            if (!skinned_mesh || skinned_mesh->type != ASSET_TYPE_MESH)
                continue;

            DrawMesh(skinned_mesh, Translate(s->position) * bone.local_to_world);
        }
    }

    BindMaterial(g_view.vertex_material);
    BindColor(COLOR_BONE);
    for (int bone_index=0; bone_index<s->bone_count; bone_index++)
        DrawEditorSkeletonBone(s, bone_index, position);
}

static void EditorSkeletonDraw(AssetData* a) {
    SkeletonData* es = (SkeletonData*)a;
    assert(es);
    assert(es->type == ASSET_TYPE_SKELETON);
    DrawEditorSkeleton(es, es->position, a->selected && !a->editing);
}

int HitTestBone(SkeletonData* s, const Vec2& world_pos) {
    float best_dist = F32_MAX;
    int best_bone_index = -1;
    for (int bone_index=0; bone_index<s->bone_count; bone_index++) {
        BoneData* eb = s->bones + bone_index;

        Mat3 local_to_world = Translate(GetAssetData()->position) * eb->local_to_world * Rotate(eb->transform.rotation);
        if (!OverlapPoint(g_view.bone_collider, world_pos, local_to_world * Scale(eb->length)))
            continue;

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

static void ParseBonePosition(BoneData& eb, Tokenizer& tk)
{
    float x;
    if (!ExpectFloat(tk, &x))
        ThrowError("misssing 'x' in bone position");
    float y;
    if (!ExpectFloat(tk, &y))
        ThrowError("misssing 'y' in bone position");

    eb.transform.position = {x,y};
}

static void ParseBoneRotation(BoneData& eb, Tokenizer& tk) {
    float r;
    if (!ExpectFloat(tk, &r))
        ThrowError("misssing bone rotation value");

    eb.transform.rotation = r;
}

static void ParseBoneLength(BoneData& eb, Tokenizer& tk) {
    float l;
    if (!ExpectFloat(tk, &l))
        ThrowError("misssing bone length value");

    eb.length = l;
}

static void ParseBone(SkeletonData* es, Tokenizer& tk) {
    if (!ExpectQuotedString(tk))
        ThrowError("expected bone name as quoted string");

    const Name* bone_name = GetName(tk);

    int parent_index = -1;
    if (!ExpectInt(tk, &parent_index))
        ThrowError("expected parent index");

    BoneData& bone = es->bones[es->bone_count++];
    bone.name = bone_name;
    bone.parent_index = parent_index;
    bone.index = es->bone_count - 1;
    bone.transform.scale = VEC2_ONE;
    bone.length = 0.25f;

    while (!IsEOF(tk))
    {
        if (ExpectIdentifier(tk, "p"))
            ParseBonePosition(bone, tk);
        else if (ExpectIdentifier(tk, "r"))
            ParseBoneRotation(bone, tk);
        else if (ExpectIdentifier(tk, "l"))
            ParseBoneLength(bone, tk);
        else
            break;
    }
}

static void LoadSkeletonData(AssetData* a) {
    assert(a);
    assert(a->type == ASSET_TYPE_SKELETON);
    SkeletonData* es = (SkeletonData*)a;

    std::filesystem::path path = a->path;
    std::string contents = ReadAllText(ALLOCATOR_DEFAULT, path);
    Tokenizer tk;
    Init(tk, contents.c_str());

    while (!IsEOF(tk)) {
        if (ExpectIdentifier(tk, "b")) {
            ParseBone(es, tk);
        } else {
            char error[1024];
            GetString(tk, error, sizeof(error) - 1);
            ThrowError("unknown identifier '%s' in skeleton", error);
        }
    }

    UpdateTransforms(es);
}

static void SaveSkeletonData(AssetData* a, const std::filesystem::path& path) {
    assert(a);
    assert(a->type == ASSET_TYPE_SKELETON);
    SkeletonData* es = (SkeletonData*)a;
    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);

    for (int i=0; i<es->bone_count; i++) {
        const BoneData& eb = es->bones[i];
        WriteCSTR(stream, "b \"%s\" %d p %f %f r %f l %f\n",
            eb.name->value,
            eb.parent_index,
            eb.transform.position.x,
            eb.transform.position.y,
            eb.transform.rotation,
            eb.length);
    }

    SaveStream(stream, path);
    Free(stream);
}

AssetData* NewEditorSkeleton(const std::filesystem::path& path) {
    const char* default_mesh = "b \"root\" -1 p 0 0\n";

    std::filesystem::path full_path = path.is_relative() ?  std::filesystem::current_path() / "assets" / "skeletons" / path : path;
    full_path += ".skel";

    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);
    WriteCSTR(stream, default_mesh);
    SaveStream(stream, full_path);
    Free(stream);

    //return LoadEditorSkeleton(full_path);
    return nullptr;
}

void UpdateTransforms(SkeletonData* s) {
    if (s->bone_count <= 0)
        return;

    BoneData& root = s->bones[0];
    root.local_to_world = Translate(root.transform.position);
    root.world_to_local = Inverse(root.local_to_world);

    for (int bone_index=1; bone_index<s->bone_count; bone_index++) {
        BoneData& bone = s->bones[bone_index];
        BoneData& parent = s->bones[bone.parent_index];
        bone.local_to_world = parent.local_to_world * Translate(bone.transform.position);
        bone.world_to_local = Inverse(bone.local_to_world);
    }

    Vec2 root_position = TransformPoint(s->bones[0].local_to_world);
    Bounds2 bounds = Bounds2 { root_position, root_position };
    for (int i=0; i<s->bone_count; i++) {
        BoneData* b = s->bones + i;
        float bone_width = b->length * BONE_WIDTH;
        Mat3 bone_transform = b->local_to_world * Rotate(b->transform.rotation);
        bounds = Union(bounds, TransformPoint(b->local_to_world));
        bounds = Union(bounds, TransformPoint(bone_transform, Vec2{b->length, 0}));
        bounds = Union(bounds, TransformPoint(bone_transform, Vec2{bone_width, bone_width}));
        bounds = Union(bounds, TransformPoint(bone_transform, Vec2{bone_width, -bone_width}));
    }

    for (int i=0; i<s->skinned_mesh_count; i++) {
        BoneData& bone = s->bones[s->skinned_meshes[i].bone_index];
        MeshData* skinned_mesh = s->skinned_meshes[i].mesh;
        if (!skinned_mesh || skinned_mesh->type != ASSET_TYPE_MESH)
            continue;

        Bounds2 mesh_bounds = Translate(GetBounds(skinned_mesh), TransformPoint(bone.local_to_world));
        bounds = Union(bounds, {mesh_bounds.min, mesh_bounds.max});
    }

    s->bounds = Expand(bounds, BOUNDS_PADDING);
}

static void LoadSkeletonMetaData(AssetData* a, Props* meta) {
    assert(a);
    assert(a->type == ASSET_TYPE_SKELETON);
    SkeletonData* s = (SkeletonData*)a;

    for (auto& key : meta->GetKeys("skin")) {
        std::string bones = meta->GetString("skin", key.c_str(), "");
        Tokenizer tk;
        Init(tk, bones.c_str());

        int bone_index = -1;
        while (ExpectInt(tk, &bone_index))
        {
            s->skinned_meshes[s->skinned_mesh_count++] = {
                GetName(key.c_str()),
                nullptr,
                bone_index
            };

            if (!ExpectDelimiter(tk, ','))
                break;
        }
    }
}

static void SkeletonDataPostLoad(AssetData* a) {
    assert(a);
    assert(a->type == ASSET_TYPE_SKELETON);
    SkeletonData* s = (SkeletonData*)a;

    for (int i=0; i<s->skinned_mesh_count; i++) {
        SkinnedMesh& sm = s->skinned_meshes[i];
        sm.mesh = (MeshData*)GetAssetData(ASSET_TYPE_MESH, sm.asset_name);
    }

    SortSkin(s);
}

int FindBoneIndex(SkeletonData* s, const Name* name) {
    for (int i=0; i<s->bone_count; i++)
        if (s->bones[i].name == name)
            return i;

    return -1;
}

static int CompareBoneParentIndex(void const* p, void const* arg) {
    BoneData* a = (BoneData*)p;
    BoneData* b = (BoneData*)arg;
    return a->parent_index - b->parent_index;
}

static void ReparentBoneTransform(BoneData& b, BoneData& p) {
    Mat3 new_local = p.world_to_local * b.local_to_world;

    b.transform.position.x = new_local.m[6];
    b.transform.position.y = new_local.m[7];

    // Scale (magnitude of basis vectors)
    f32 scale_x = Sqrt(
        new_local.m[0] * new_local.m[0] +
        new_local.m[1] * new_local.m[1]);
    f32 scale_y = Sqrt(
        new_local.m[3] * new_local.m[3] +
        new_local.m[4] * new_local.m[4]);

    b.transform.scale = Vec2(scale_x, scale_y);
    b.transform.rotation = Degrees(
        Atan2(
            new_local.m[1] / scale_x,
            new_local.m[0] / scale_x));
}

int ReparentBone(SkeletonData* s, int bone_index, int parent_index) {
    BoneData& eb = s->bones[bone_index];

    eb.parent_index = parent_index;

    qsort(s->bones, s->bone_count, sizeof(BoneData), CompareBoneParentIndex);

    int bone_map[MAX_BONES];
    for (int i=0; i<s->bone_count; i++)
        bone_map[s->bones[i].index] = i;

    for (int i=1; i<s->bone_count; i++)
    {
        s->bones[i].parent_index = bone_map[s->bones[i].parent_index];
        s->bones[i].index = i;
    }

    for (int i=0; i<s->skinned_mesh_count; i++)
    {
        SkinnedMesh& esm = s->skinned_meshes[i];
        esm.bone_index = bone_map[esm.bone_index];
    }

    ReparentBoneTransform(s->bones[bone_map[bone_index]], s->bones[bone_map[parent_index]]);
    UpdateTransforms(s);

    return bone_map[bone_index];
}

void RemoveBone(SkeletonData* s, int bone_index) {
    if (bone_index <= 0 || bone_index >= s->bone_count)
        return;

    BoneData& eb = s->bones[bone_index];
    int parent_index = eb.parent_index;

    // Reparent children to parent
    for (int i=0; i<s->bone_count; i++)
    {
        BoneData& child = s->bones[i];
        if (child.parent_index == bone_index)
        {
            child.parent_index = parent_index;
            ReparentBoneTransform(child, s->bones[parent_index]);
        }
    }

    // Remove any skinned meshes attached to this bone
    for (int i=0; i<s->skinned_mesh_count; )
    {
        SkinnedMesh& esm = s->skinned_meshes[i];
        if (esm.bone_index == bone_index)
        {
            s->skinned_meshes[i] = s->skinned_meshes[--s->skinned_mesh_count];
        }
        else
            i++;
    }

    s->bone_count--;

    for (int i=bone_index; i<s->bone_count; i++)
    {
        BoneData& enb = s->bones[i];
        enb = s->bones[i + 1];
        enb.index = i;
        if (enb.parent_index == bone_index)
            enb.parent_index = parent_index;
        else if (enb.parent_index > bone_index)
            enb.parent_index--;
    }

    for (int i=0; i<s->skinned_mesh_count; i++)
    {
        SkinnedMesh& esm = s->skinned_meshes[i];
        if (esm.bone_index > bone_index)
            esm.bone_index--;
    }

    UpdateTransforms(s);
}

const Name* GetUniqueBoneName(SkeletonData* s) {
    const Name* bone_name = GetName("Bone");

    int bone_postfix = 2;
    while (FindBoneIndex(s, bone_name) != -1)
    {
        char name[64];
        Format(name, sizeof(name), "Bone%d", bone_postfix++);
        bone_name = GetName(name);
    }

    return bone_name;
}

void Serialize(SkeletonData* s, Stream* stream) {
    const Name* bone_names[MAX_BONES];
    for (int i=0; i<s->bone_count; i++)
        bone_names[i] = s->bones[i].name;

    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE_SKELETON;
    header.version = 1;
    header.flags = 0;
    header.names = s->bone_count;
    WriteAssetHeader(stream, &header, bone_names);

    WriteU8(stream, (u8)s->bone_count);

    for (int i=0; i<s->bone_count; i++) {
        BoneData& eb = s->bones[i];
        WriteI8(stream, (char)eb.parent_index);
        WriteStruct(stream, eb.local_to_world);
        WriteStruct(stream, eb.world_to_local);
        WriteStruct(stream, eb.transform.position);
        WriteFloat(stream, eb.transform.rotation);
        WriteStruct(stream, eb.transform.scale);
    }
}

Skeleton* ToSkeleton(Allocator* allocator, SkeletonData* es, const Name* name) {
    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 8192);
    if (!stream)
        return nullptr;
    Serialize(es, stream);
    SeekBegin(stream, 0);

    Skeleton* skeleton = (Skeleton*)LoadAssetInternal(allocator, name, ASSET_SIGNATURE_SKELETON, LoadSkeleton, stream);
    Free(stream);

    return skeleton;
}

static void EditorSkeletonSaveMetadata(AssetData* a, Props* meta) {
    assert(a);
    assert(a->type == ASSET_TYPE_SKELETON);
    SkeletonData* es = (SkeletonData*)a;
    meta->ClearGroup("skin");

    for (int i=0; i<es->skinned_mesh_count; i++)
    {
        if (es->skinned_meshes[i].mesh == nullptr)
            continue;

        const Name* mesh_name = es->skinned_meshes[i].asset_name;
        std::string value = meta->GetString("skin", mesh_name->value, "");
        if (!value.empty())
            value += ", ";
        value += std::to_string(es->skinned_meshes[i].bone_index);
        meta->SetString("skin", mesh_name->value, value.c_str());
    }
}

static bool EditorSkeletonOverlapPoint(AssetData* a, const Vec2& position, const Vec2& overlap_point) {
    assert(a);
    assert(a->type == ASSET_TYPE_SKELETON);
    SkeletonData* es = (SkeletonData*)a;
    return Contains(es->bounds + position, overlap_point);
}

static bool EditorSkeletonOverlapBounds(AssetData* a, const Bounds2& overlap_bounds) {
    assert(a);
    assert(a->type == ASSET_TYPE_SKELETON);
    SkeletonData* es = (SkeletonData*)a;
    return Intersects(es->bounds + a->position, overlap_bounds);
}

static void SkeletonUndoRedo(AssetData* a) {
    assert(a);
    assert(a->type == ASSET_TYPE_SKELETON);
    SkeletonData* s = (SkeletonData*)a;
    UpdateTransforms(s);
}

static void EditorSkeletonSortOrderChanged(AssetData* a) {
    assert(a);
    assert(a->type == ASSET_TYPE_SKELETON);
    SkeletonData* es = (SkeletonData*)a;
    SortSkin(es);
}

static void Init(SkeletonData* s) {
    assert(s);

    s->vtable = {
        .load = LoadSkeletonData,
        .post_load = SkeletonDataPostLoad,
        .save = SaveSkeletonData,
        .load_metadata = LoadSkeletonMetaData,
        .save_metadata = EditorSkeletonSaveMetadata,
        .draw = EditorSkeletonDraw,
        .overlap_point = EditorSkeletonOverlapPoint,
        .overlap_bounds = EditorSkeletonOverlapBounds,
        .undo_redo = SkeletonUndoRedo,
        .on_sort_order_changed = EditorSkeletonSortOrderChanged
    };

    InitSkeletonEditor(s);
}

void InitSkeletonData(AssetData* ea) {
    assert(ea);
    assert(ea->type == ASSET_TYPE_SKELETON);
    SkeletonData* es = (SkeletonData*)ea;
    Init(es);
}
