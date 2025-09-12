//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "../asset/editor_asset.h"

namespace fs = std::filesystem;

static void ImportAnimation(const fs::path& source_path, Stream* output_stream, Props* config, Props* meta)
{
    AssetHeader header = {};
    header.signature = ASSET_SIGNATURE_ANIMATION;
    header.version = 1;
    WriteAssetHeader(output_stream, &header);

    EditorAnimation* en = LoadEditorAnimation(ALLOCATOR_DEFAULT, source_path);
    if (!en)
        throw std::runtime_error("invalid animation");

    EditorSkeleton* es = LoadEditorSkeleton(ALLOCATOR_DEFAULT, std::string(en->skeleton_name->value) + ".skel");
    if (!es)
        throw std::runtime_error("invalid skeleton");
}

static bool DoesAnimationDependOn(const std::filesystem::path& path, const std::filesystem::path& dependency_path)
{
    if (dependency_path.extension() != ".skel")
        return false;

    EditorAnimation* en = LoadEditorAnimation(ALLOCATOR_DEFAULT, path);
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
}

static const char* g_animation_extensions[] = {
    ".anim",
    nullptr
};

static AssetImporterTraits g_animation_importer_traits = {
    .type_name = "Animation",
    .signature = ASSET_SIGNATURE_ANIMATION,
    .file_extensions = g_animation_extensions,
    .import_func = ImportAnimation,
    .does_depend_on = DoesAnimationDependOn,
};

AssetImporterTraits* GetAnimationImporterTraits()
{
    return &g_animation_importer_traits;
}























#if 0
/*

NoZ Game Engine

        Copyright(c) 2025 NoZ Games, LLC

*/

#include <noz/binary_stream.h>
#include "AnimationImporter.h"
#include "../gltf_loader.h"

using namespace noz;

AnimationImporter::AnimationImporter(const ImportConfig::ModelConfig& config)
    : _config(config)
{
}

bool AnimationImporter::can_import(const string& filePath) const
{
    // Check if it's a GLB file
    if (filePath.length() < 4 || filePath.substr(filePath.length() - 4) != ".glb")
        return false;

    return meta_file::parse(filePath + ".meta").get_bool("Mesh", "importAnimation", false);
}

vector<string> AnimationImporter::get_supported_extensions() const
{
    return { ".glb" };
}

string AnimationImporter::get_name() const
{
    return "AnimationImporter";
}

void AnimationImporter::import(const string& sourcePath, const string& outputDir)
{
    filesystem::path sourceFile(sourcePath);
    string fileName = sourceFile.stem().string();
    gltf_loader loader;
    if (!loader.open(sourcePath))
        throw runtime_error("invalid glb file");

    auto bones = loader.read_bones(gltf_loader::BoneFilter::fromMetaFile(sourcePath + ".meta"));
    auto animation = loader.read_animation(bones, fileName);
    loader.close();

    if (!animation)
        throw runtime_error("no animations found in GLB file");

    write_animation(outputDir + "/" + fileName + ".animation", animation, sourcePath);
}

void AnimationImporter::write_animation(
        const string& outputPath,
    const shared_ptr<gltf_animation>& animation,
    const string& sourcePath)
{
    binary_stream stream;
    stream.write_signature("ANIM");
    stream.write_uint32(1);
    stream.write_uint16(animation->frame_count);
    stream.write_uint16(animation->frame_stride);
    stream.write_vector<animation_track>(animation->tracks);
    stream.write_vector<float>(animation->data);

    // Write to file
    filesystem::path outputFile(outputPath);
    filesystem::create_directories(outputFile.parent_path());
    stream.save(outputPath);
}
#endif