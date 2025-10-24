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

struct EditorVfx : AssetData
{
    VfxFloat duration;
    bool loop;
    EditorVfxEmitter emitters[MAX_EMITTERS_PER_VFX];
    int emitter_count;
    Vfx* vfx;
    VfxHandle handle;
};

extern void InitEditorVfx(AssetData* ea);
extern EditorVfx* LoadEditorVfx(const std::filesystem::path& path);
extern Vfx* ToVfx(Allocator* allocator, EditorVfx* evfx, const Name* name);
extern void Serialize(EditorVfx* evfx, Stream* stream);
extern EditorVfx* Clone(Allocator* allocator, EditorVfx* evfx);
extern void DrawEditorVfx(AssetData* ea);