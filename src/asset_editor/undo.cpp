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
    RingBuffer* undo;
    RingBuffer* redo;
    int next_group_id;
    int current_group_id;
};

static UndoSystem g_undo = {};

void Undo()
{
    if (IsEmpty(g_undo.undo))
        return;

    int group_id = ((UndoItem*)GetBack(g_undo.undo))->group_id;

    while (!IsEmpty(g_undo.undo))
    {
        UndoItem& item = *(UndoItem*)GetBack(g_undo.undo);
        if (item.group_id != -1 && item.group_id != group_id)
            break;


        UndoItem& redo = *(UndoItem*)PushBack(g_undo.redo);
        redo = item;
        redo.saved = Clone(ALLOCATOR_DEFAULT, *item.ea);

        Copy(*item.ea, *item.saved);
        Free(item.saved);
        PopBack(g_undo.undo);

        if (item.group_id == -1)
            break;
    }
}

void Redo()
{
    if (IsEmpty(g_undo.redo))
        return;

    int group_id = ((UndoItem*)GetBack(g_undo.redo))->group_id;

    while (!IsEmpty(g_undo.redo))
    {
        UndoItem& item = *(UndoItem*)GetBack(g_undo.redo);
        if (item.group_id != -1 && item.group_id != group_id)
            break;

        UndoItem& undo = *(UndoItem*)PushBack(g_undo.undo);
        undo = item;
        undo.saved = Clone(ALLOCATOR_DEFAULT, *item.ea);

        Copy(*item.ea, *item.saved);
        Free(item.saved);
        PopBack(g_undo.redo);

        if (item.group_id == -1)
            break;
    }
}


void CancelUndo()
{
    int group_id = ((UndoItem*)GetBack(g_undo.undo))->group_id;

    while (!IsEmpty(g_undo.undo))
    {
        UndoItem& item = *(UndoItem*)GetBack(g_undo.undo);
        if (item.group_id != -1 && item.group_id != group_id)
            break;

        Free(item.saved);
        item.ea = nullptr;
        item.saved = nullptr;
        PopBack(g_undo.undo);

        if (item.group_id == -1)
            break;
    }
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
    if (IsFull(g_undo.undo))
    {
        UndoItem& old = *(UndoItem*)GetFront(g_undo.undo);
        Free(old.saved);
        old.ea = nullptr;
        old.saved = nullptr;
        PopBack(g_undo.undo);
    }

    UndoItem& item = *(UndoItem*)PushBack(g_undo.undo);
    item.group_id = g_undo.current_group_id;
    item.ea = &ea;
    item.saved = Clone(ALLOCATOR_DEFAULT, ea);

    // Clear the redo
    while (!IsEmpty(g_undo.redo))
    {
        UndoItem& old = *(UndoItem*)GetFront(g_undo.redo);
        Free(old.saved);
        old.ea = nullptr;
        old.saved = nullptr;
        PopBack(g_undo.redo);
    }
}

void InitUndo()
{
    assert(!g_undo.undo);
    g_undo.undo = CreateRingBuffer(ALLOCATOR_DEFAULT, sizeof(UndoItem), MAX_UNDO);
    g_undo.redo = CreateRingBuffer(ALLOCATOR_DEFAULT, sizeof(UndoItem), MAX_UNDO);
    g_undo.current_group_id = 1;
    g_undo.next_group_id = 1;
}

void ShutdownUndo()
{
    assert(g_undo.undo);
    Free(g_undo.undo);
    Free(g_undo.redo);
    g_undo = {};
}
