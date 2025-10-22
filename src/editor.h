//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

// @constants
constexpr int MAX_ASSETS = 1024;
constexpr int MAX_VIEWS = 16;
constexpr int MAX_ASSET_PATHS = 8;
constexpr float BONE_WIDTH = 0.10f;
constexpr float BONE_DEFAULT_LENGTH = 0.25f;
constexpr float BONE_ORIGIN_SIZE = 0.12f;
constexpr float BOUNDS_PADDING = 0.01f;

struct LogView;
struct View;
struct TextInputBox;
struct AssetImporter;

struct Editor {
    LogView* log_view;
    View* view_stack[MAX_VIEWS];
    u32 view_stack_count = 0;
    TextInputBox* command_input;
    TextInputBox* search_input;
    bool command_mode;
    bool search_mode;
    bool auto_quit;
    int fps;
    bool stats_requested;
    const char* exe;
    char asset_paths[MAX_ASSET_PATHS][4096];
    int asset_path_count;
    AssetImporter* importers;
    std::filesystem::file_time_type config_timestamp;
    std::filesystem::path output_dir;

    PoolAllocator* asset_allocator;
    int sorted_assets[MAX_ASSETS];
};

extern Editor g_editor;

// @editor
extern View* GetView();
extern void PushView(View* view);
extern void PopView();

// @command
constexpr int MAX_COMMAND_ARGS = 4;
constexpr int MAX_COMMAND_ARG_SIZE = 128;

struct Command
{
    const Name* name;
    int arg_count;
    char args[MAX_COMMAND_ARGS][MAX_COMMAND_ARG_SIZE];
};

extern void InitCommands();
extern bool ParseCommand(const char* str, Command& command);
extern const char* GetVarTypeNameFromSignature(AssetSignature signature);

// @import
struct ImportEvent {
    const Name* name;
    AssetSignature signature;
    std::filesystem::path target_path;
};

extern void QueueImport(const std::filesystem::path& path);
extern void WaitForImportJobs();

// @grid
extern Vec2 SnapToGrid(const Vec2& position, bool secondary);

// @ui
extern Color GetButtonHoverColor(ElementState state, float time, void* user_data);
extern void UpdateConfirmDialog();
extern void ShowConfirmDialog(const char* message, const std::function<void()>& callback);

#include "asset_editor.h"
#include "view.h"
#include "import/asset_importer.h"

