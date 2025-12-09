//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

namespace fs = std::filesystem;

static void ImportAnimation(AssetData* ea, const std::filesystem::path& path, Props* config, Props* meta) {
    (void)config;
    (void)meta;

    assert(ea);
    assert(ea->type == ASSET_TYPE_ANIMATION);
    AnimationData* en = (AnimationData*)ea;

    SkeletonData* es = (SkeletonData*)GetAssetData(ASSET_TYPE_SKELETON, en->skeleton_name);
    if (!es)
        ThrowError("invalid skeleton");

    Stream* stream = CreateStream(nullptr, 4096);
    Serialize(en, stream, es);
    SaveStream(stream, path);
    Free(stream);
}

static bool DoesAnimationDependOn(AssetData* ea, AssetData* dependency)
{
    assert(ea);
    assert(ea->type == ASSET_TYPE_ANIMATION);
    assert(dependency);

    if (dependency->type != ASSET_TYPE_SKELETON)
        return false;

#if 0
    AnimationData* en = LoadEditorAnimation(ea->path);
    if (!en)
        return false;

    // does the path end with en->skeleton_name + ".skel"?
    std::string expected_path = en->skeleton_name->value;
    std::string path_str = dependency_path.string();
    expected_path += ".skel";

    CleanPath(expected_path.data());
    CleanPath(path_str.data());

    bool result = path_str.ends_with(expected_path);

    Free(en);

    return result;
#else
    return false;
#endif
}

AssetImporter GetAnimationImporter()
{
    return {
        .type = ASSET_TYPE_ANIMATION,
        .ext = ".anim",
        .import_func = ImportAnimation,
        .does_depend_on = DoesAnimationDependOn,
    };
}
