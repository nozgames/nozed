//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

class Props;

struct AssetImporter
{
    AssetType type;
    const char* ext;
    void (*import_func) (AssetData* ea, Stream* output_stream, Props* config, Props* meta);
    bool (*does_depend_on) (AssetData* ea, AssetData* dependency);
};
