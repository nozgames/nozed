//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

constexpr int STATE_STACK_SIZE = 16;

constexpr int UI_REF_WIDTH = 1920;
constexpr int UI_REF_HEIGHT = 1080;

#include <asset_editor.h>

enum ViewState
{
    VIEW_STATE_DEFAULT,
    VIEW_STATE_MOVE,
    VIEW_STATE_EDIT,
    VIEW_STATE_BOX_SELECT,
    VIEW_STATE_COMMAND,
};

enum ViewDrawMode
{
    VIEW_DRAW_MODE_WIREFRAME,
    VIEW_DRAW_MODE_SOLID,
    VIEW_DRAW_MODE_SHADED
};

typedef const Name* (*PreviewCommandFunc)(const Command& command);

struct ViewVtable
{
    void (*update)();
    void (*draw)();
    Bounds2 (*bounds)();
    void (*shutdown)();
    void (*rename)(const Name* new_name);
    const Name* (*preview_command)(const Command& command);
    bool (*allow_text_input)();
};

struct Shortcut;

struct View
{
    ViewState state_stack[STATE_STACK_SIZE];
    int state_stack_count;
    Camera* camera;
    Material* shaded_material;
    Material* solid_material;
    Material* vertex_material;
    Material* editor_material;
    Mesh* vertex_mesh;
    Mesh* arrow_mesh;
    Mesh* circle_mesh;
    Mesh* arc_mesh[101];
    Mesh* edge_mesh;
    Collider* bone_collider;
    float zoom;
    float zoom_ref_scale;
    float select_size;
    float ui_scale;
    float dpi;
    InputSet* input;
    InputSet* input_command;
    bool clear_selection_on_release;
    Vec2 pan_position_camera;
    Vec2 pan_position;
    Command command;
    const Name* command_preview;

    u32 selected_asset_count;
    int sorted_assets[MAX_ASSETS];

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
    ViewDrawMode draw_mode;
};

extern View g_view;

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
extern void FocusAsset(EditorAsset* ea);
extern void HandleRename(const Name* name);
extern void AddEditorAsset(EditorAsset* ea);
inline EditorAsset* GetSortedEditorAsset(int index) { return GetEditorAsset(g_view.sorted_assets[index]); }
inline EditorAsset* GetEditingAsset() { return GetEditorAsset(g_view.edit_asset_index); }

// @grid
extern void InitGrid(Allocator* allocator);
extern void ShutdownGrid();
extern void DrawGrid(Camera* camera);

// @undo
extern void InitUndo();
extern void ShutdownUndo();
extern void HandleCommand(const Command& command);
extern void RecordUndo(EditorAsset* ea);
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
extern void DrawRect(const Rect& rect);
extern void DrawLine(const Vec2& v0, const Vec2& v1);
extern void DrawLine(const Vec2& v0, const Vec2& v1, f32 width);
extern void DrawVertex(const Vec2& v);
extern void DrawVertex(const Vec2& v, f32 size);
extern void DrawArrow(const Vec2& v, const Vec2& dir);
extern void DrawArrow(const Vec2& v, const Vec2& dir, f32 size);
extern void DrawOrigin(EditorAsset* ea);
extern void DrawBounds(EditorAsset* ea, float expand=0, const Color& color=COLOR_BLACK);
extern void DrawBone(const Vec2& a, const Vec2& b);
extern void DrawBone(const Mat3& transform, const Mat3& parent_transform, const Vec2& position, float length=BONE_DEFAULT_LENGTH);
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

constexpr Color COLOR_VERTEX_SELECTED = Color32ToColor(255, 121, 0, 255);
constexpr Color COLOR_VERTEX = COLOR_BLACK;
constexpr Color COLOR_EDGE = COLOR_BLACK;
constexpr Color COLOR_EDGE_SELECTED = Color32ToColor(253, 151, 11, 255);
constexpr Color COLOR_ORIGIN = Color32ToColor(255, 159, 44, 255);
constexpr Color COLOR_ORIGIN_BORDER = { 0,0,0,1 };

constexpr Color COLOR_SELECTED = { 1,1,1,1 };
constexpr Color COLOR_CENTER = { 1, 1, 1, 0.5f};
constexpr Color COLOR_UI_BACKGROUND = Color24ToColor(0x343c4a);
constexpr Color COLOR_UI_TEXT = Color24ToColor(0xc5c5cb);
constexpr Color COLOR_UI_BUTTON_HOVER = Color24ToColor(0x76a8ff);
constexpr Color COLOR_UI_BUTTON = {0.9f, 0.9f, 0.9f, 1.0f};
constexpr Color COLOR_UI_BUTTON_TEXT = COLOR_UI_BACKGROUND;
