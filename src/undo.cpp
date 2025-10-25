//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

constexpr int MAX_UNDO = MAX_ASSETS * 2;

struct UndoItem {
    FatAssetData saved_asset;
    AssetData* asset;
    int group_id;
};

struct UndoSystem {
    RingBuffer* undo;
    RingBuffer* redo;
    int next_group_id;
    int current_group_id;
    AssetData* temp[MAX_UNDO];
    int temp_count;
};

static UndoSystem g_undo = {};

inline UndoItem* GetBackItem(RingBuffer* buffer) {
    return (UndoItem*)GetBack(buffer);
}

inline int GetBackGroupId(RingBuffer* buffer) {
    return GetBackItem(buffer)->group_id;
}

static void Free(UndoItem& item) {
    item.group_id = -1;
}

static void CallUndoRedo() {
    for (int i=0; i<g_undo.temp_count; i++) {
        AssetData* ea = g_undo.temp[i];
        if (ea->vtable.undo_redo)
            ea->vtable.undo_redo(ea);
    }

    SortAssets();

    g_undo.temp_count = 0;
}

static bool UndoInternal(bool allow_redo)
{
    if (IsEmpty(g_undo.undo))
        return false;

    int group_id = GetBackGroupId(g_undo.undo);

    while (!IsEmpty(g_undo.undo)) {
        UndoItem* item = GetBackItem(g_undo.undo);
        if (item->group_id != -1 && item->group_id != group_id)
            break;

        AssetData* undo_asset = item->asset;
        assert(undo_asset);
        assert(undo_asset->type == item->saved_asset.asset.type);

        if (allow_redo) {
            UndoItem* redo_item = static_cast<UndoItem*>(PushBack(g_undo.redo));
            redo_item->group_id = group_id;
            redo_item->asset = item->asset;
            Clone(&redo_item->saved_asset.asset, undo_asset);
        }

        Clone(undo_asset, &item->saved_asset.asset);

        g_undo.temp[g_undo.temp_count++] = undo_asset;

        PopBack(g_undo.undo);

        if (item->group_id == -1)
            break;
    }

    CallUndoRedo();

    return true;
}

bool Undo()
{
    return UndoInternal(true);
}

bool Redo()
{
    if (IsEmpty(g_undo.redo))
        return false;

    int group_id = ((UndoItem*)GetBack(g_undo.redo))->group_id;

    while (!IsEmpty(g_undo.redo))
    {
        UndoItem& redo_item = *(UndoItem*)GetBack(g_undo.redo);
        if (redo_item.group_id != -1 && redo_item.group_id != group_id)
            break;

        AssetData* redo_asset = redo_item.asset;
        assert(redo_asset);
        assert(redo_asset->type == redo_item.saved_asset.asset.type);

        UndoItem& undo_item = *(UndoItem*)PushBack(g_undo.undo);
        undo_item.group_id = group_id;
        undo_item.asset = redo_item.asset;
        Clone(&undo_item.saved_asset.asset, redo_asset);

        Clone(redo_asset, &redo_item.saved_asset.asset);

        g_undo.temp[g_undo.temp_count++] = redo_asset;

        PopBack(g_undo.redo);

        if (redo_item.group_id == -1)
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

void EndUndoGroup() {
    g_undo.current_group_id = -1;
}

void RecordUndo() {
    RecordUndo(GetAssetData());
}

void RecordUndo(AssetData* a) {
    // Maxium undo size
    if (IsFull(g_undo.undo)) {
        UndoItem& old = *(UndoItem*)GetFront(g_undo.undo);
        Free(old);
        PopBack(g_undo.undo);
    }

    UndoItem& item = *(UndoItem*)PushBack(g_undo.undo);
    item.group_id = g_undo.current_group_id;
    item.asset = a;
    Clone(&item.saved_asset.asset, a);

    // Clear the redo
    while (!IsEmpty(g_undo.redo)) {
        UndoItem& old = *(UndoItem*)GetFront(g_undo.redo);
        Free(old);
        PopBack(g_undo.redo);
    }
}

void RemoveFromUndoRedo(AssetData* a) {
    for (u32 i=GetCount(g_undo.undo); i>0; i--) {
        UndoItem& undo_item = *(UndoItem*)GetAt(g_undo.undo, i-1);
        if (undo_item.asset != a) continue;
        RemoveAt(g_undo.undo, i);
    }

    for (u32 i=GetCount(g_undo.redo); i>0; i--) {
        UndoItem& undo_item = *(UndoItem*)GetAt(g_undo.redo, i-1);
        if (undo_item.asset != a) continue;
        RemoveAt(g_undo.redo, i);
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
