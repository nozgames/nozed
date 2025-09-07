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