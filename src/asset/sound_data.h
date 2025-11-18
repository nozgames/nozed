//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

struct SoundData : AssetData {
    SoundHandle handle;
    Sound* sound;
};

extern void InitSoundData(AssetData* a);
