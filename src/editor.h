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
constexpr float BOUNDS_PADDING = 0.02f;

struct TextInputBox;
struct AssetImporter;
struct AssetData;

// @tool
struct ToolVtable {
    void (*cancel)();
    void (*update)();
    void (*draw)();
};

enum ToolType {
    TOOL_TYPE_NONE,
    TOOL_TYPE_BOX_SELECT,
    TOOL_TYPE_MOVE,
    TOOL_TYPE_ROTATE,
    TOOL_TYPE_SCALE,
    TOOL_TYPE_SELECT,
    TOOL_TYPE_WEIGHT,
};

struct Tool {
    ToolType type = TOOL_TYPE_NONE;
    ToolVtable vtable = {};
    InputSet* input;
    bool inherit_input;
};

void BeginTool(const Tool& tool);
void CancelTool();
void EndTool();

struct Editor {
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

    AssetData* editing_asset;

    Tool tool;
};

extern Editor g_editor;

// @command
constexpr int MAX_COMMAND_ARGS = 4;
constexpr int MAX_COMMAND_ARG_SIZE = 128;

struct Command {
    const Name* name;
    int arg_count;
    char args[MAX_COMMAND_ARGS][MAX_COMMAND_ARG_SIZE];
};

struct CommandHandler {
    const Name* short_name;
    const Name* name;
    void (*handler)(const Command&);
};

struct CommandInputOptions {
    const CommandHandler* commands;
    const char* prefix;
    const char* placeholder;
    bool hide_empty;
};

extern void InitCommandInput();
extern void ShutdownCommandInput();
extern void BeginCommandInput(const CommandInputOptions& options);
extern void UpdateCommandInput();
extern void EndCommandInput();
extern const char* GetVarTypeNameFromSignature(AssetSignature signature);

// @import
struct ImportEvent {
    const Name* name;
    AssetSignature signature;
    std::filesystem::path target_path;
};

extern void InitImporter();
extern void ShutdownImporter();
extern void UpdateImporter();
extern void QueueImport(const std::filesystem::path& path);
extern void WaitForImportJobs();

extern AssetImporter GetShaderImporter();
extern AssetImporter GetTextureImporter();
extern AssetImporter GetFontImporter();
extern AssetImporter GetMeshImporter();
extern AssetImporter GetVfxImporter();
extern AssetImporter GetSoundImporter();
extern AssetImporter GetSkeletonImporter();
extern AssetImporter GetAnimationImporter();

// @grid
extern Vec2 SnapToGrid(const Vec2& position, bool secondary=false);

// @ui
extern Color GetButtonHoverColor(ElementState state, float time, void* user_data);
extern void UpdateConfirmDialog();
extern void ShowConfirmDialog(const char* message, const std::function<void()>& callback);

// @editor
inline AssetData* GetAssetData() { return g_editor.editing_asset; }
inline bool IsToolActive() { return g_editor.tool.type != TOOL_TYPE_NONE; }
extern void HandleUndo();
extern void HandleRedo();

// @server
extern void InitEditorServer(Props* config);
extern void UpdateEditorServer();
extern void ShutdownEditorServer();

#include "asset/asset_data.h"
#include "view.h"
#include "import/asset_importer.h"
#include "tool.h"
