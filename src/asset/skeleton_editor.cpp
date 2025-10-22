//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#include "../asset_editor.h"
#include "utils/file_helpers.h"

static void Init(EditorSkeleton* es);
extern Asset* LoadAssetInternal(Allocator* allocator, const Name* asset_name, AssetSignature signature, AssetLoaderFunc loader, Stream* stream);

void DrawEditorSkeletonBone(EditorSkeleton* es, int bone_index, const Vec2& position) {
    EditorBone* eb = es->bones + bone_index;
    Mat3 local_to_world = eb->local_to_world * Rotate(eb->transform.rotation);
    DrawBone(
        local_to_world,
        GetParentLocalToWorld(es, eb, local_to_world),
        position,
        eb->length);
}

void DrawEditorSkeleton(EditorSkeleton* es, const Vec2& position, bool selected) {
    UpdateTransforms(es);

    if (g_view.draw_mode != VIEW_DRAW_MODE_WIREFRAME) {
        BindColor(COLOR_WHITE);
        for (int i=0; i<es->skinned_mesh_count; i++) {
            EditorBone& bone = es->bones[es->skinned_meshes[i].bone_index];
            EditorMesh* skinned_mesh = es->skinned_meshes[i].mesh;
            if (!skinned_mesh || skinned_mesh->type != EDITOR_ASSET_TYPE_MESH)
                continue;

            DrawMesh(skinned_mesh, Translate(es->position) * bone.local_to_world);
        }
    }

    BindMaterial(g_view.vertex_material);
    BindColor(selected ? COLOR_SELECTED : COLOR_BLACK);
    for (int bone_index=0; bone_index<es->bone_count; bone_index++)
        DrawEditorSkeletonBone(es, bone_index, position);
}

static void EditorSkeletonDraw(EditorAsset* ea) {
    EditorSkeleton* es = (EditorSkeleton*)ea;
    assert(es);
    assert(es->type == EDITOR_ASSET_TYPE_SKELETON);
    DrawEditorSkeleton(es, es->position, ea->selected && !ea->editing);
}

int HitTestBone(EditorSkeleton* es, const Vec2& world_pos) {
    const float size = g_view.select_size;
    float best_dist = F32_MAX;
    int best_bone_index = -1;
    for (int bone_index=0; bone_index<es->bone_count; bone_index++) {
        EditorBone& bone = es->bones[bone_index];
        Mat3 local_to_world = bone.local_to_world * Rotate(bone.transform.rotation);
        Vec2 bone_position = TransformPoint(local_to_world);
        float dist = Length(bone_position - world_pos);
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
    for (int bone_index=0; bone_index<es->bone_count; bone_index++) {
        EditorBone* eb = es->bones + bone_index;

        Vec2 collider_point = TransformPoint(Rotate(-eb->transform.rotation), TransformPoint(eb->world_to_local, world_pos));
        if (!OverlapPoint(g_view.bone_collider, collider_point, eb->length))
            continue;

        Mat3 local_to_world = eb->local_to_world * Rotate(eb->transform.rotation);
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

static void ParseBonePosition(EditorBone& eb, Tokenizer& tk)
{
    float x;
    if (!ExpectFloat(tk, &x))
        ThrowError("misssing 'x' in bone position");
    float y;
    if (!ExpectFloat(tk, &y))
        ThrowError("misssing 'y' in bone position");

    eb.transform.position = {x,y};
}

static void ParseBoneRotation(EditorBone& eb, Tokenizer& tk) {
    float r;
    if (!ExpectFloat(tk, &r))
        ThrowError("misssing bone rotation value");

    eb.transform.rotation = r;
}

static void ParseBoneLength(EditorBone& eb, Tokenizer& tk) {
    float l;
    if (!ExpectFloat(tk, &l))
        ThrowError("misssing bone length value");

    eb.length = l;
}

static void ParseBone(EditorSkeleton* es, Tokenizer& tk) {
    if (!ExpectQuotedString(tk))
        ThrowError("expected bone name as quoted string");

    const Name* bone_name = GetName(tk);

    int parent_index = -1;
    if (!ExpectInt(tk, &parent_index))
        ThrowError("expected parent index");

    EditorBone& bone = es->bones[es->bone_count++];
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

static void EditorSkeletonLoad(EditorAsset* ea)
{
    assert(ea);
    assert(ea->type == EDITOR_ASSET_TYPE_SKELETON);
    EditorSkeleton* es = (EditorSkeleton*)ea;

    std::filesystem::path path = ea->path;
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

static void EditorSkeletonSave(EditorAsset* ea, const std::filesystem::path& path) {
    assert(ea);
    assert(ea->type == EDITOR_ASSET_TYPE_SKELETON);
    EditorSkeleton* es = (EditorSkeleton*)ea;
    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 4096);

    for (int i=0; i<es->bone_count; i++) {
        const EditorBone& eb = es->bones[i];
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

EditorAsset* NewEditorSkeleton(const std::filesystem::path& path)
{
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

void UpdateTransforms(EditorSkeleton* es) {
    if (es->bone_count <= 0)
        return;

    EditorBone& root = es->bones[0];
    root.local_to_world = Translate(root.transform.position);
    root.world_to_local = Inverse(root.local_to_world);

    for (int bone_index=1; bone_index<es->bone_count; bone_index++) {
        EditorBone& bone = es->bones[bone_index];
        EditorBone& parent = es->bones[bone.parent_index];
        bone.local_to_world = parent.local_to_world * Translate(bone.transform.position);
        bone.world_to_local = Inverse(bone.local_to_world);
    }

    Vec2 root_position = TransformPoint(es->bones[0].local_to_world);
    Bounds2 bounds = Bounds2 { root_position, root_position };
    for (int i=0; i<es->bone_count; i++) {
        EditorBone* eb = es->bones + i;
        float bone_width = eb->length * BONE_WIDTH;
        Mat3 bone_transform = eb->local_to_world * Rotate(eb->transform.rotation);
        bounds = Union(bounds, TransformPoint(eb->local_to_world));
        bounds = Union(bounds, TransformPoint(bone_transform, Vec2{eb->length, 0}));
        bounds = Union(bounds, TransformPoint(bone_transform, Vec2{bone_width, bone_width}));
        bounds = Union(bounds, TransformPoint(bone_transform, Vec2{bone_width, -bone_width}));
    }

    for (int i=0; i<es->skinned_mesh_count; i++) {
        EditorBone& bone = es->bones[es->skinned_meshes[i].bone_index];
        EditorMesh* skinned_mesh = es->skinned_meshes[i].mesh;
        if (!skinned_mesh || skinned_mesh->type != EDITOR_ASSET_TYPE_MESH)
            continue;

        Bounds2 mesh_bounds = Translate(GetBounds(skinned_mesh), TransformPoint(bone.local_to_world));
        bounds = Union(bounds, {mesh_bounds.min, mesh_bounds.max});
    }

    es->bounds = Expand(bounds, BOUNDS_PADDING);
}

static void EditorSkeletonLoadMetadata(EditorAsset* ea, Props* meta) {
    assert(ea);
    assert(ea->type == EDITOR_ASSET_TYPE_SKELETON);
    EditorSkeleton* es = (EditorSkeleton*)ea;

    for (auto& key : meta->GetKeys("skin")) {
        std::string bones = meta->GetString("skin", key.c_str(), "");
        Tokenizer tk;
        Init(tk, bones.c_str());

        int bone_index = -1;
        while (ExpectInt(tk, &bone_index))
        {
            es->skinned_meshes[es->skinned_mesh_count++] = {
                GetName(key.c_str()),
                nullptr,
                bone_index
            };

            if (!ExpectDelimiter(tk, ','))
                break;
        }
    }
}

static void EditorSkeletonPostLoad(EditorAsset* ea)
{
    assert(ea);
    assert(ea->type == EDITOR_ASSET_TYPE_SKELETON);
    EditorSkeleton* es = (EditorSkeleton*)ea;

    for (int i=0; i<es->skinned_mesh_count; i++)
    {
        EditorSkinnedMesh& esm = es->skinned_meshes[i];
        esm.mesh = (EditorMesh*)GetEditorAsset(EDITOR_ASSET_TYPE_MESH, esm.asset_name);
    }
}

int FindBoneIndex(EditorSkeleton* es, const Name* name)
{
    for (int i=0; i<es->bone_count; i++)
        if (es->bones[i].name == name)
            return i;

    return -1;
}

static int CompareBoneParentIndex(void const* p, void const* arg)
{
    EditorBone* a = (EditorBone*)p;
    EditorBone* b = (EditorBone*)arg;
    return a->parent_index - b->parent_index;
}

static void ReparentBoneTransform(EditorBone& b, EditorBone& p)
{
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

int ReparentBone(EditorSkeleton* es, int bone_index, int parent_index)
{
    EditorBone& eb = es->bones[bone_index];

    eb.parent_index = parent_index;

    qsort(es->bones, es->bone_count, sizeof(EditorBone), CompareBoneParentIndex);

    int bone_map[MAX_BONES];
    for (int i=0; i<es->bone_count; i++)
        bone_map[es->bones[i].index] = i;

    for (int i=1; i<es->bone_count; i++)
    {
        es->bones[i].parent_index = bone_map[es->bones[i].parent_index];
        es->bones[i].index = i;
    }

    for (int i=0; i<es->skinned_mesh_count; i++)
    {
        EditorSkinnedMesh& esm = es->skinned_meshes[i];
        esm.bone_index = bone_map[esm.bone_index];
    }

    ReparentBoneTransform(es->bones[bone_map[bone_index]], es->bones[bone_map[parent_index]]);
    UpdateTransforms(es);

    return bone_map[bone_index];
}

void RemoveBone(EditorSkeleton* es, int bone_index)
{
    if (bone_index <= 0 || bone_index >= es->bone_count)
        return;

    EditorBone& eb = es->bones[bone_index];
    int parent_index = eb.parent_index;

    // Reparent children to parent
    for (int i=0; i<es->bone_count; i++)
    {
        EditorBone& child = es->bones[i];
        if (child.parent_index == bone_index)
        {
            child.parent_index = parent_index;
            ReparentBoneTransform(child, es->bones[parent_index]);
        }
    }

    // Remove any skinned meshes attached to this bone
    for (int i=0; i<es->skinned_mesh_count; )
    {
        EditorSkinnedMesh& esm = es->skinned_meshes[i];
        if (esm.bone_index == bone_index)
        {
            es->skinned_meshes[i] = es->skinned_meshes[--es->skinned_mesh_count];
        }
        else
            i++;
    }

    es->bone_count--;

    for (int i=bone_index; i<es->bone_count; i++)
    {
        EditorBone& enb = es->bones[i];
        enb = es->bones[i + 1];
        enb.index = i;
        if (enb.parent_index == bone_index)
            enb.parent_index = parent_index;
        else if (enb.parent_index > bone_index)
            enb.parent_index--;
    }

    for (int i=0; i<es->skinned_mesh_count; i++)
    {
        EditorSkinnedMesh& esm = es->skinned_meshes[i];
        if (esm.bone_index > bone_index)
            esm.bone_index--;
    }

    UpdateTransforms(es);
}

const Name* GetUniqueBoneName(EditorSkeleton* es)
{
    const Name* bone_name = GetName("Bone");

    int bone_postfix = 2;
    while (FindBoneIndex(es, bone_name) != -1)
    {
        char name[64];
        Format(name, sizeof(name), "Bone%d", bone_postfix++);
        bone_name = GetName(name);
    }

    return bone_name;
}

void Serialize(EditorSkeleton* es, Stream* stream)
{
    const Name* bone_names[MAX_BONES];
    for (int i=0; i<es->bone_count; i++)
        bone_names[i] = es->bones[i].name;

    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE_SKELETON;
    header.version = 1;
    header.flags = 0;
    header.names = es->bone_count;
    WriteAssetHeader(stream, &header, bone_names);

    WriteU8(stream, (u8)es->bone_count);

    for (int i=0; i<es->bone_count; i++)
    {
        EditorBone& eb = es->bones[i];
        WriteI8(stream, (char)eb.parent_index);
        WriteStruct(stream, eb.local_to_world);
        WriteStruct(stream, eb.world_to_local);
        WriteStruct(stream, eb.transform.position);
        WriteFloat(stream, eb.transform.rotation);
        WriteStruct(stream, eb.transform.scale);
    }
}

Skeleton* ToSkeleton(Allocator* allocator, EditorSkeleton* es, const Name* name)
{
    Stream* stream = CreateStream(ALLOCATOR_DEFAULT, 8192);
    if (!stream)
        return nullptr;
    Serialize(es, stream);
    SeekBegin(stream, 0);

    Skeleton* skeleton = (Skeleton*)LoadAssetInternal(allocator, name, ASSET_SIGNATURE_SKELETON, LoadSkeleton, stream);
    Free(stream);

    return skeleton;
}

static void EditorSkeletonSaveMetadata(EditorAsset* ea, Props* meta)
{
    assert(ea);
    assert(ea->type == EDITOR_ASSET_TYPE_SKELETON);
    EditorSkeleton* es = (EditorSkeleton*)ea;
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

static bool EditorSkeletonOverlapPoint(EditorAsset* ea, const Vec2& position, const Vec2& overlap_point)
{
    assert(ea);
    assert(ea->type == EDITOR_ASSET_TYPE_SKELETON);
    EditorSkeleton* es = (EditorSkeleton*)ea;
    return Contains(es->bounds + position, overlap_point);
}

static bool EditorSkeletonOverlapBounds(EditorAsset* ea, const Bounds2& overlap_bounds)
{
    assert(ea);
    assert(ea->type == EDITOR_ASSET_TYPE_SKELETON);
    EditorSkeleton* es = (EditorSkeleton*)ea;
    return Intersects(es->bounds + ea->position, overlap_bounds);
}

static void EditorSkeletonUndoRedo(EditorAsset* ea) {
    assert(ea);
    assert(ea->type == EDITOR_ASSET_TYPE_SKELETON);
    EditorSkeleton* es = (EditorSkeleton*)ea;
    UpdateTransforms(es);
}

extern void SkeletonViewInit();
extern void SkeletonViewDraw();
extern void SkeletonViewUpdate();

static void Init(EditorSkeleton* es) {
    assert(es);

    es->vtable = {
        .load = EditorSkeletonLoad,
        .post_load = EditorSkeletonPostLoad,
        .save = EditorSkeletonSave,
        .load_metadata = EditorSkeletonLoadMetadata,
        .save_metadata = EditorSkeletonSaveMetadata,
        .draw = EditorSkeletonDraw,
        .view_init = SkeletonViewInit,
        .overlap_point = EditorSkeletonOverlapPoint,
        .overlap_bounds = EditorSkeletonOverlapBounds,
        .undo_redo = EditorSkeletonUndoRedo
    };
}

void InitEditorSkeleton(EditorAsset* ea) {
    assert(ea);
    assert(ea->type == EDITOR_ASSET_TYPE_SKELETON);
    EditorSkeleton* es = (EditorSkeleton*)ea;
    Init(es);
}
