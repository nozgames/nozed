//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

struct EventData : AssetData {
};

extern AssetImporter GetEventImporter();
extern void InitEventData(AssetData* a);
extern AssetData* NewEventData(const std::filesystem::path& path);
