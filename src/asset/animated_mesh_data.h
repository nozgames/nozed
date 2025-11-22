//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

#include "mesh_data.h"

constexpr int ANIMATED_MESH_MAX_FRAMES = 32;

struct AnimatedMeshData : AssetData {
    MeshData frames[ANIMATED_MESH_MAX_FRAMES];
    int frame_count;
};