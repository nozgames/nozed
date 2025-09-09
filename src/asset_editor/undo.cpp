//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"

#define MAX_UNDO 256

struct UndoItem
{
    EditableAsset* ea;
    EditableAsset* saved;
    int group_id;
};

struct UndoSystem
{
    RingBuffer* buffer;
    int next_group_id;
    int current_group_id;
};

static UndoSystem g_undo = {};

void Undo()
{
    if (IsEmpty(g_undo.buffer))
        return;

    int group_id = ((UndoItem*)GetBack(g_undo.buffer))->group_id;

    while (!IsEmpty(g_undo.buffer))
    {
        UndoItem& item = *(UndoItem*)GetBack(g_undo.buffer);
        if (item.group_id != -1 && item.group_id != group_id)
            break;

        Copy(*item.ea, *item.saved);

        Free(item.saved);
        item.ea = nullptr;
        item.saved = nullptr;
        PopBack(g_undo.buffer);

        if (item.group_id == -1)
            break;
    }
}

void CancelUndo()
{
    int group_id = ((UndoItem*)GetBack(g_undo.buffer))->group_id;

    while (!IsEmpty(g_undo.buffer))
    {
        UndoItem& item = *(UndoItem*)GetBack(g_undo.buffer);
        if (item.group_id != -1 && item.group_id != group_id)
            break;

        Free(item.saved);
        item.ea = nullptr;
        item.saved = nullptr;
        PopBack(g_undo.buffer);

        if (item.group_id == -1)
            break;
    }
}

void Redo()
{
}

void BeginUndoGroup()
{
    g_undo.current_group_id = g_undo.next_group_id++;
}

void EndUndoGroup()
{
    g_undo.current_group_id = -1;
}

void RecordUndo(EditableAsset& ea)
{
    // Maxium undo size
    if (IsFull(g_undo.buffer))
    {
        UndoItem& old = *(UndoItem*)GetFront(g_undo.buffer);
        Free(old.saved);
        old.ea = nullptr;
        old.saved = nullptr;
        PopBack(g_undo.buffer);
    }

    UndoItem& item = *(UndoItem*)PushBack(g_undo.buffer);
    item.group_id = g_undo.current_group_id;
    item.ea = &ea;
    item.saved = Clone(ALLOCATOR_DEFAULT, ea);
}

void InitUndo()
{
    assert(!g_undo.buffer);
    g_undo.buffer = CreateRingBuffer(ALLOCATOR_DEFAULT, sizeof(UndoItem), MAX_UNDO);
    g_undo.current_group_id = 1;
    g_undo.next_group_id = 1;
}

void ShutdownUndo()
{
    assert(g_undo.buffer);
    Free(g_undo.buffer);
    g_undo = {};
}
