//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"

#define MAX_UNDO 256

struct Undo
{
    RingBuffer* buffer;
};

static Undo g_undo = {};

void Undo()
{
}

void Redo()
{
}

void PushUndo(EditableMesh* mesh)
{
}

void InitUndo()
{
    g_undo.buffer = CreateRingBuffer(ALLOCATOR_DEFAULT, sizeof(EditableMesh*), MAX_UNDO);
}

void ShutdownUndo()
{

}
