//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include <editor.h>

#define MAX_UNDO 256

struct UndoItem
{
    EditorAssetData ea;
    int group_id;
};

struct UndoSystem
{
    RingBuffer* undo;
    RingBuffer* redo;
    int next_group_id;
    int current_group_id;
    int temp[MAX_UNDO];
    int temp_count;
};

static UndoSystem g_undo = {};

static void Free(UndoItem& item)
{
    item.group_id = -1;
}

static void CallUndoRedo()
{
    for (int i=0; i<g_undo.temp_count; i++)
    {
        EditorAsset* ea = GetEditorAsset(g_undo.temp[i]);
        if (ea->vtable.undo_redo)
            ea->vtable.undo_redo(ea);
    }

    g_undo.temp_count = 0;
}

static bool UndoInternal(bool allow_redo)
{
    if (IsEmpty(g_undo.undo))
        return false;

    int group_id = ((UndoItem*)GetBack(g_undo.undo))->group_id;

    while (!IsEmpty(g_undo.undo))
    {
        UndoItem& undo = *(UndoItem*)GetBack(g_undo.undo);
        if (undo.group_id != -1 && undo.group_id != group_id)
            break;

        if (allow_redo)
        {
            UndoItem& redo = *(UndoItem*)PushBack(g_undo.redo);
            redo.ea = *(EditorAssetData*)GetEditorAsset(undo.ea.asset.index);
            redo.group_id = group_id;
        }

        EditorAsset* ea = GetEditorAsset(undo.ea.asset.index);
        assert(ea);
        *(EditorAssetData*)ea = undo.ea;

        g_undo.temp[g_undo.temp_count++] = undo.ea.asset.index;

        PopBack(g_undo.undo);

        if (undo.group_id == -1)
            break;
    }

    CallUndoRedo();

    return true;
}

bool Undo()
{
    return UndoInternal(true);;
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
        undo.ea = *(EditorAssetData*)GetEditorAsset(redo.ea.asset.index);
        undo.group_id = group_id;

        *(EditorAssetData*)GetEditorAsset(redo.ea.asset.index) = redo.ea;

        g_undo.temp[g_undo.temp_count++] = redo.ea.asset.index;

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

    UndoInternal(false);
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

void RecordUndo(EditorAsset* ea)
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
    item.ea = *(EditorAssetData*)ea;

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
    g_undo.current_group_id = -1;
    g_undo.next_group_id = 1;
}

void ShutdownUndo()
{
    assert(g_undo.undo);
    Free(g_undo.undo);
    Free(g_undo.redo);
    g_undo = {};
}
