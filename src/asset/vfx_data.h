//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

#include "../../../src/vfx/vfx_internal.h"

constexpr int MAX_EMITTERS_PER_VFX = 32;


struct EditorVfxEmitter
{
    const Name* name;
    VfxEmitterDef def;
};

struct VfxData : AssetData {
    VfxFloat duration;
    bool loop;
    EditorVfxEmitter emitters[MAX_EMITTERS_PER_VFX];
    int emitter_count;
    Vfx* vfx;
    VfxHandle handle;
    bool playing;
};

extern void InitVfxData(AssetData* ea);
extern VfxData* LoadEditorVfx(const std::filesystem::path& path);
extern Vfx* ToVfx(Allocator* allocator, VfxData* v, const Name* name);
extern void Serialize(VfxData* v, Stream* stream);
extern VfxData* Clone(Allocator* allocator, VfxData* evfx);
extern void DrawEditorVfx(AssetData* ea);
extern AssetData* NewVfxData(const std::filesystem::path& path);
