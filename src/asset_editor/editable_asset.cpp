//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"
#include "file_helpers.h"

extern EditableMesh* LoadEditableMesh(Allocator* allocator, const std::filesystem::path& filename);

EditableAsset* CreateEditableAsset(const std::filesystem::path& path)
{
    std::error_code ec;
    std::filesystem::path relative_path = std::filesystem::relative(path, "assets", ec);
    relative_path.replace_extension("");
    relative_path = FixSlashes(relative_path);

    EditableAsset* ea = (EditableAsset*)Alloc(ALLOCATOR_DEFAULT, sizeof(EditableAsset));
    ea->name = GetName(relative_path.string().c_str());
    return ea;
}

i32 LoadEditableAssets(EditableAsset** assets)
{
    i32 asset_count = 0;
    for (auto& asset_path : GetFilesInDirectory("assets"))
    {
        std::filesystem::path ext = asset_path.extension();
        if (ext == ".glb")
        {
            EditableAsset* ea = CreateEditableAsset(asset_path);
            assets[asset_count++] = ea;
            ea->mesh = LoadEditableMesh(ALLOCATOR_DEFAULT, asset_path.string().c_str());
        }
    }

    return 0;
}

