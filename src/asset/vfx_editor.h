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

struct EditorVfx
{
    VfxFloat duration;
    bool loop;
    EditorVfxEmitter emitters[MAX_EMITTERS_PER_VFX];
    int emitter_count;
    Vfx* vfx;
    VfxHandle handle;
};

extern EditorAsset* CreateEditableVfxAsset(const std::filesystem::path& path, EditorVfx* evfx);
extern EditorVfx* LoadEditorVfx(Allocator* allocator, const std::filesystem::path& path);
extern Vfx* ToVfx(Allocator* allocator, const EditorVfx& evfx, const Name* name);
extern void Serialize(const EditorVfx& evfx, Stream* stream);
extern EditorVfx* Clone(Allocator* allocator, const EditorVfx& evfx);
extern void DrawEditorVfx(EditorAsset& ea);