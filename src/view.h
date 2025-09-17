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

struct ViewVtable
{
    ViewRenameFunc rename;
    PreviewCommandFunc preview_command;
};

struct Shortcut;

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

    ViewVtable vtable;

    int edit_asset_index;

    Shortcut* shortcuts;
    bool show_names;
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
extern EditorAsset& GetEditingAsset();
extern void AddEditorAsset(EditorAsset* ea);

// @grid
extern void InitGrid(Allocator* allocator);
extern void ShutdownGrid();
extern void DrawGrid(Camera* camera);

// @undo
extern void InitUndo();
extern void ShutdownUndo();
extern void HandleCommand(const Command& command);
extern void RecordUndo(EditorAsset& ea);
extern void RecordUndo();
extern void BeginUndoGroup();
extern void EndUndoGroup();
extern bool Undo();
extern bool Redo();
extern void CancelUndo();

// @notifications
extern void InitNotifications();
extern void UpdateNotifications();
extern void AddNotification(const char* format, ...);

// @mesh_editor
extern void HandleBoxSelect(const Bounds2& bounds);

// @draw
extern void DrawLine(const Vec2& v0, const Vec2& v1);
extern void DrawLine(const Vec2& v0, const Vec2& v1, f32 width);
extern void DrawVertex(const Vec2& v);
extern void DrawVertex(const Vec2& v, f32 size);
extern void DrawOrigin(const EditorAsset& ea);
extern void DrawOrigin(const Vec2& position);
extern void DrawBounds(EditorAsset& ea, float expand=0);
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
