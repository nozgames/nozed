//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

struct TextureData : AssetData {
    Texture* texture;
    Material* material;
    float scale;
};

extern void InitTextureData(AssetData* a);
extern void UpdateBounds(TextureData* t);
