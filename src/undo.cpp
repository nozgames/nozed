//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include <editor.h>

#define MAX_UNDO 256

struct UndoItem
{
    EditorAsset* ea;
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

static void Free(UndoItem& item)
{
    if (item.ea)
        Free(item.ea);
    item.ea = nullptr;
    item.group_id = -1;
}

bool Undo()
{
    if (IsEmpty(g_undo.undo))
        return false;

    int group_id = ((UndoItem*)GetBack(g_undo.undo))->group_id;

    while (!IsEmpty(g_undo.undo))
    {
        UndoItem& undo = *(UndoItem*)GetBack(g_undo.undo);
        if (undo.group_id != -1 && undo.group_id != group_id)
            break;

        UndoItem& redo = *(UndoItem*)PushBack(g_undo.redo);
        redo.ea = GetEditorAsset(undo.ea->index);
        redo.group_id = group_id;

        g_view.assets[undo.ea->index] = undo.ea;

        PopBack(g_undo.undo);

        if (undo.group_id == -1)
            break;
    }

    return true;
}

bool Redo()
{
    if (IsEmpty(g_undo.redo))
        return false;

    int group_id = ((UndoItem*)GetBack(g_undo.redo))->group_id;

    while (!IsEmpty(g_undo.redo))
    {
        UndoItem& redo = *(UndoItem*)GetBack(g_undo.redo);
        if (redo.group_id != -1 && redo.group_id != group_id)
            break;

        UndoItem& undo = *(UndoItem*)PushBack(g_undo.undo);
        undo.ea = GetEditorAsset(redo.ea->index);
        undo.group_id = group_id;

        g_view.assets[undo.ea->index] = redo.ea;
        PopBack(g_undo.redo);

        if (redo.group_id == -1)
            break;
    }

    return true;
}

void CancelUndo()
{
    if (GetCount(g_undo.undo) == 0)
        return;

    int group_id = ((UndoItem*)GetBack(g_undo.undo))->group_id;

    while (!IsEmpty(g_undo.undo))
    {
        UndoItem& item = *(UndoItem*)GetBack(g_undo.undo);
        if (item.group_id != -1 && item.group_id != group_id)
            break;

        Free(item);
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

void RecordUndo()
{
    RecordUndo(GetEditingAsset());
}

void RecordUndo(EditorAsset& ea)
{
    // Maxium undo size
    if (IsFull(g_undo.undo))
    {
        UndoItem& old = *(UndoItem*)GetFront(g_undo.undo);
        Free(old);
        PopBack(g_undo.undo);
    }

    UndoItem& item = *(UndoItem*)PushBack(g_undo.undo);
    item.group_id = g_undo.current_group_id;
    item.ea = Clone(ALLOCATOR_DEFAULT, ea);

    // Clear the redo
    while (!IsEmpty(g_undo.redo))
    {
        UndoItem& old = *(UndoItem*)GetFront(g_undo.redo);
        Free(old);
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
