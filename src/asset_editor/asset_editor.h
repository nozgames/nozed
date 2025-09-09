//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

constexpr int STATE_STACK_SIZE = 16;
constexpr int MAX_ASSETS = 1024;

constexpr int UI_REF_WIDTH = 1920;
constexpr int UI_REF_HEIGHT = 1080;

struct View;

#include "../editor_mesh.h"

enum EditableAssetType
{
    EDITABLE_ASSET_TYPE_UNKNOWN = -1,
    EDITABLE_ASSET_TYPE_MESH,
    EDITABLE_ASSET_TYPE_COUNT,
};

struct EditableAsset
{
    const Name* name;
    EditableAssetType type;
    EditorMesh* mesh;
    Vec2 position;
    Vec2 saved_position;
    bool dirty;
    std::filesystem::path path;
    bool selected;
};

enum AssetEditorState
{
    ASSET_EDITOR_STATE_DEFAULT,
    ASSET_EDITOR_STATE_MOVE,
    ASSET_EDITOR_STATE_EDIT,
    ASSET_EDITOR_STATE_BOX_SELECT,
    ASSET_EDITOR_STATE_PAN,
};

struct AssetEditor
{
    AssetEditorState state_stack[STATE_STACK_SIZE];
    int state_stack_count;
    Camera* camera;
    Material* material;
    Material* vertex_material;
    Mesh* vertex_mesh;
    Mesh* edge_mesh;
    float zoom;
    float zoom_ref_scale;
    float ui_scale;
    float dpi;
    InputSet* input;
    InputSet* command_input;
    int selected_vertex;
    int edit_asset_index;
    bool clear_selection_on_release;
    Vec2 pan_start;
    bool command_palette;

    EditableAsset* assets[MAX_ASSETS];
    u32 asset_count;
    u32 selected_asset_count;

    Bounds2 box_selection;
    Vec2 move_world_position;

    bool drag;
    Vec2 drag_position;
    Vec2 drag_world_position;
    Vec2 drag_delta;
    Vec2 drag_world_delta;
    Vec2 mouse_position;
    Vec2 mouse_world_position;
};

#include "editor_assets.h"


extern AssetEditor g_asset_editor;

constexpr Color COLOR_SELECTED = { 1,1,1,1 };

// @editor
extern void InitAssetEditor();
extern void UpdateAssetEditor();
extern void ShutdownAssetEditor();
extern void ClearBoxSelect();
extern void PushState(AssetEditorState state);
extern void PopState();
extern void FocusAsset(int asset_index);

// @grid
extern void InitGrid(Allocator* allocator);
extern void ShutdownGrid();
extern void DrawGrid(Camera* camera);

// @undo
extern void InitUndo();
extern void ShutdownUndo();
extern void HandleCommand(const std::string& str);
extern void RecordUndo(EditableAsset& ea);
extern void BeginUndoGroup();
extern void EndUndoGroup();
extern void Undo();
extern void Redo();
extern void CancelUndo();

// @editable_asset
extern EditableAsset* CreateEditableMeshAsset(const std::filesystem::path& path);
extern i32 LoadEditableAssets(EditableAsset** assets);
extern void SaveEditableAssets();
extern bool HitTestAsset(const EditableAsset& ea, const Vec2& hit_pos);
extern bool HitTestAsset(const EditableAsset& ea, const Bounds2& hit_bounds);
extern int HitTestAssets(const Vec2& hit_pos);
extern int HitTestAssets(const Bounds2& bit_bounds);
extern void DrawEdges(const EditableAsset& ea, int min_edge_count, Color color);
extern void DrawAsset(const EditableAsset& ea);
extern Bounds2 GetBounds(const EditableAsset& ea);
extern int GetFirstSelectedAsset();
extern Bounds2 GetSelectedBounds(const EditableAsset& ea);
extern void MoveTo(EditableAsset& asset, const Vec2& position);
extern void ClearAssetSelection();
extern void SetAssetSelection(int asset_index);
extern void AddAssetSelection(int asset_index);
extern int FindAssetByName(const Name* name);
extern EditableAsset* Clone(Allocator* allocator, const EditableAsset& ea);
extern void Copy(EditableAsset& dst, const EditableAsset& src);

// @notifications
extern void InitNotifications();
extern void UpdateNotifications();
extern void AddNotification(const char* format, ...);

// @mesh_editor
extern void UpdateMeshEditor(EditableAsset& ea);
extern void InitMeshEditor(EditableAsset& ea);
extern void DrawMeshEditor(EditableAsset& ea);
extern void HandleMeshEditorBoxSelect(EditableAsset& ea, const Bounds2& bounds);

// @draw
extern void DrawLine(const Vec2& v0, const Vec2& v1, f32 width);
extern void DrawVertex(const Vec2& v, f32 size);
