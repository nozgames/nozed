//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

#include "mesh_data.h"

struct RuntimeAnimatedMeshData {
    MeshData frames[ANIMATED_MESH_MAX_FRAMES];
};

struct AnimatedMeshData : AssetData {
    RuntimeAnimatedMeshData* data;
    MeshData* frames;
    int frame_count;
    int current_frame;
};

extern void InitAnimatedMeshData(AssetData* a);
extern AssetData* NewAnimatedMeshData(const std::filesystem::path& path);
extern AnimatedMesh* ToAnimatedMesh(AnimatedMeshData* m);