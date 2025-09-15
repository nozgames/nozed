//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

constexpr int STATE_STACK_SIZE = 16;
constexpr int MAX_ASSETS = 1024;

constexpr int UI_REF_WIDTH = 1920;
constexpr int UI_REF_HEIGHT = 1080;

#include <asset_editor.h>
#include <editor_assets.h>

enum ViewState
{
    VIEW_STATE_DEFAULT,
    VIEW_STATE_MOVE,
    VIEW_STATE_EDIT,
    VIEW_STATE_BOX_SELECT
};

typedef void (*ViewRenameFunc)(const Name* new_name);
typedef const Name* (*PreviewCommandFunc)(const Command& command);
typedef void (*UndoRedoFunc)();

struct ViewVtable
{
    ViewRenameFunc rename;
    PreviewCommandFunc preview_command;
    UndoRedoFunc undo_redo;
};

struct View
{
    ViewState state_stack[STATE_STACK_SIZE];
    int state_stack_count;
    Camera* camera;
    Material* material;
    Material* vertex_material;
    Mesh* vertex_mesh;
    Mesh* edge_mesh;
    Collider* bone_collider;
    float zoom;
    float zoom_ref_scale;
    float select_size;
    float ui_scale;
    float dpi;
    InputSet* input;
    InputSet* command_input;
    int edit_asset_index;
    bool clear_selection_on_release;
    Vec2 pan_position_camera;
    Vec2 pan_position;
    Command command;
    bool command_palette;
    const Name* command_preview;

    EditorAsset* assets[MAX_ASSETS];
    u32 asset_count;
    u32 selected_asset_count;

    Bounds2 box_selection;
    void (*box_select_callback)(const Bounds2& bounds);
    Vec2 move_world_position;

    bool drag;
    Vec2 drag_position;
    Vec2 drag_world_position;
    Vec2 drag_delta;
    Vec2 drag_world_delta;
    Vec2 mouse_position;
    Vec2 mouse_world_position;

    Vec2 light_dir;

    ViewVtable* vtable;
};

extern View g_view;

constexpr Color COLOR_SELECTED = { 1,1,1,1 };
constexpr Color COLOR_EDGE = { 0,0,0, 0.5f };
constexpr Color COLOR_VERTEX = { 0,0,0,1 };
constexpr Color COLOR_CENTER = { 1, 1, 1, 0.5f};
constexpr Color COLOR_ORIGIN = { 1.0f, 159.0f / 255.0f, 44.0f / 255.0f, 1};
constexpr Color COLOR_ORIGIN_BORDER = { 0,0,0,1 };

// @view
extern void InitView();
extern void UpdateView();
extern void ShutdownView();
extern void InitViewUserConfig(Props* user_config);
extern void SaveViewUserConfig(Props* user_config);
extern void BeginBoxSelect(void (*callback)(const Bounds2& bounds));
extern void ClearBoxSelect();
extern void PushState(ViewState state);
extern void PopState();
extern void FocusAsset(int asset_index);
extern void HandleRename(const Name* name);

// @grid
extern void InitGrid(Allocator* allocator);
extern void ShutdownGrid();
extern void DrawGrid(Camera* camera);

// @undo
extern void InitUndo();
extern void ShutdownUndo();
extern void HandleCommand(const Command& command);
extern void RecordUndo(EditorAsset& ea);
extern void BeginUndoGroup();
extern void EndUndoGroup();
extern bool Undo();
extern bool Redo();
extern void CancelUndo();

// @editable_asset
extern EditorAsset* LoadEditorMeshAsset(const std::filesystem::path& path);
extern EditorAsset* LoadEditorVfxAsset(const std::filesystem::path& path);
extern void LoadEditorAssets();
extern void SaveEditorAssets();
extern bool HitTestAsset(const EditorAsset& ea, const Vec2& hit_pos);
extern bool HitTestAsset(const EditorAsset& ea, const Vec2& position, const Vec2& hit_pos);
extern bool HitTestAsset(const EditorAsset& ea, const Bounds2& hit_bounds);
extern int HitTestAssets(const Vec2& hit_pos);
extern int HitTestAssets(const Bounds2& bit_bounds);
extern void DrawEdges(const EditorAsset& ea, int min_edge_count, Color color);
extern void DrawAsset(EditorAsset& ea);
extern Bounds2 GetBounds(const EditorAsset& ea);
extern int GetFirstSelectedAsset();
extern Bounds2 GetSelectedBounds(const EditorAsset& ea);
extern void MoveTo(EditorAsset& asset, const Vec2& position);
extern void ClearAssetSelection();
extern void SetAssetSelection(int asset_index);
extern void AddAssetSelection(int asset_index);
extern int FindEditorAssetByName(const Name* name);
extern EditorAsset* Clone(Allocator* allocator, const EditorAsset& ea);
extern void Copy(EditorAsset& dst, const EditorAsset& src);
extern EditorAsset* GetEditorAsset(i32 index);

// @notifications
extern void InitNotifications();
extern void UpdateNotifications();
extern void AddNotification(const char* format, ...);

// @mesh_editor
extern void UpdateMeshEditor(EditorAsset& ea);
extern void InitMeshEditor(EditorAsset& ea);
extern void DrawMeshEditor(EditorAsset& ea);
extern void HandleBoxSelect(const Bounds2& bounds);

// @skeleton_editor
extern void InitSkeletonEditor(EditorAsset& ea);
extern void UpdateSkeletonEditor();
extern void DrawSkeletonEditor();

// @animation_editor
extern void InitAnimationEditor(EditorAsset& ea);
extern void ShutdownAnimationEditor();
extern void DrawAnimationEditor();
extern void UpdateAnimationEditor();

// @draw
extern void DrawLine(const Vec2& v0, const Vec2& v1);
extern void DrawLine(const Vec2& v0, const Vec2& v1, f32 width);
extern void DrawVertex(const Vec2& v);
extern void DrawVertex(const Vec2& v, f32 size);
extern void DrawOrigin(const EditorAsset& ea);
extern void DrawOrigin(const Vec2& position);
extern void DrawBounds(const EditorAsset& ea, float expand=0);
extern void DrawBone(const Vec2& a, const Vec2& b);
extern void DrawBone(const Mat3& transform, const Mat3& parent_transform, const Vec2& position);
extern void DrawDashedLine(const Vec2& v0, const Vec2& v1, f32 width, f32 length);
extern void DrawDashedLine(const Vec2& v0, const Vec2& v1);

// @shortcut
struct Shortcut
{
    InputCode button;
    bool alt;
    bool ctrl;
    bool shift;
    void (*action)();
};

extern void EnableShortcuts(const Shortcut* shortcuts);
extern void CheckShortcuts(const Shortcut* shortcuts);
