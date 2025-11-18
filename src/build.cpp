//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

struct BuildData {
    FILE* file;
};

namespace fs = std::filesystem;

static bool BuildAsset(u32, void* item_data, void* user_data) {
    BuildData* data = static_cast<BuildData*>(user_data);
    AssetData* a = static_cast<AssetData*>(item_data);

    std::string type_upper = ToString(a->type);
    Uppercase(type_upper.data(), (u32)type_upper.size());

    std::string name_upper = a->name->value;
    Uppercase(name_upper.data(), (u32)name_upper.size());

    fprintf(data->file, "static u8 %s_%s_DATA[] = {\n", type_upper.c_str(), name_upper.c_str());

    FILE* asset_file = fopen(GetTargetPath(a).string().c_str(), "rb");
    if (asset_file) {
        char buffer[1024];
        size_t bytes_read = 0;
        while ((bytes_read = fread(buffer, 1, sizeof(buffer), asset_file)) > 0) {
            for (size_t i = 0; i < bytes_read; i++) {
                fprintf(data->file, "0x%02X,", static_cast<unsigned char>(buffer[i]));
            }
        }
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

    BuildData data = {
        .file = file
    };
    Enumerate(g_editor.asset_allocator, BuildAsset, &data);

    fprintf(file, "\n#endif\n");
    fclose(file);
}
