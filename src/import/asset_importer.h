//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

class Props;

struct AssetImporter
{
    EditorAssetType type;
    AssetSignature signature;
    const char* ext;
    void (*import_func) (EditorAsset* ea, Stream* output_stream, Props* config, Props* meta);
    bool (*does_depend_on) (EditorAsset* ea, EditorAsset* dependency);
};
