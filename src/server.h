//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

#include <string>

// @broadcast
void BroadcastAssetChange(const std::filesystem::path& path);

// @connection
bool HasConnectedClient();