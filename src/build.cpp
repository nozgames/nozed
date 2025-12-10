//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

struct BuildData {
    FILE* file;
    AssetType type;
    const char* extension;
    const char* suffix;
};

namespace fs = std::filesystem;

static bool BuildAsset(u32, void* item_data, void* user_data) {
    BuildData* data = static_cast<BuildData*>(user_data);
    AssetData* a = static_cast<AssetData*>(item_data);

    if (a->editor_only)
        return true;

    if (a->type != data->type)
        return true;

    std::string type_upper = ToString(a->type);
    Uppercase(type_upper.data(), (u32)type_upper.size());

    std::string name_upper = a->name->value;
    Uppercase(name_upper.data(), (u32)name_upper.size());

    std::string suffix_upper;
    if (data->suffix) {
        suffix_upper = data->suffix;
        Uppercase(suffix_upper.data(), (u32)suffix_upper.size());
    }

    fprintf(data->file, "static u8 %s_%s%s_DATA[] = {", type_upper.c_str(), name_upper.c_str(), suffix_upper.c_str());

    fs::path asset_path = GetTargetPath(a);
    if (data->extension)
        asset_path += data->extension;

    FILE* asset_file = fopen(asset_path.string().c_str(), "rb");
    if (asset_file) {
        char buffer[1024];
        size_t bytes_read;
        bool first = true;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), asset_file)) > 0) {
            for (size_t i = 0; i < bytes_read; i++) {
                fprintf(data->file, first ? "%u" : ",%u", static_cast<unsigned char>(buffer[i]));
                first = false;
            }
        }
        fclose(asset_file);
    }

    fprintf(data->file, "};\n\n");
    return true;
}

void Build() {
    const fs::path& manifest_path = GetManifestPath();
    fs::path build_path = manifest_path;
    build_path.replace_extension("");
    build_path = build_path.string() + "_build.cpp";

    fs::path header_path = manifest_path.filename();
    header_path.replace_extension(".h");

    FILE* file = fopen(build_path.string().c_str(), "wt");

    fprintf(file, "#include \"%s\"\n\n", header_path.string().c_str());
    fprintf(file, "#if !defined(DEBUG)\n\n");

    try
    {
        std::filesystem::create_directory(manifest_path.parent_path());
    }
    catch (...)
    {
    }

    // Iterate over all asset types
    for (int type = 0; type < ASSET_TYPE_COUNT; type++) {
        AssetType asset_type = static_cast<AssetType>(type);

        if (asset_type == ASSET_TYPE_SHADER) {
            BuildData data = { .file = file, .type = asset_type, .extension = nullptr, .suffix = nullptr };
            fprintf(file, "#ifdef NOZ_PLATFORM_GLES\n\n");
            data.extension = ".gles";
            Enumerate(g_editor.asset_allocator, BuildAsset, &data);

            fprintf(file, "#elif NOZ_PLATFORM_GL\n\n");
            data.extension = ".glsl";
            Enumerate(g_editor.asset_allocator, BuildAsset, &data);

            Enumerate(g_editor.asset_allocator, BuildAsset, &data);
            fprintf(file, "#else\n\n");

            Enumerate(g_editor.asset_allocator, BuildAsset, &data);
            fprintf(file, "#endif\n\n");

        } else {
            BuildData data = { .file = file, .type = asset_type, .extension = nullptr, .suffix = nullptr };
            Enumerate(g_editor.asset_allocator, BuildAsset, &data);
        }
    }

    fprintf(file, "\n#endif\n");
    fclose(file);
}
