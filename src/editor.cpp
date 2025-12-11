//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include <editor.h>
#include "nozed_assets.h"
#include "server.h"

namespace fs = std::filesystem;

Editor g_editor = {};
Props* g_config = nullptr;

static std::thread::id g_main_thread_id;

struct LogQueue
{
    std::mutex mutex;
    std::queue<std::string> queue;
};

static LogQueue& GetLogQueue()
{
    static LogQueue instance;
    return instance;
}

static void HandleLog(LogType type, const char* message)
{
    // Add type prefix with color for display
    std::string formatted_message;
    switch(type) {
    case LOG_TYPE_INFO:
        formatted_message = std::string(message);
        break;
    case LOG_TYPE_WARNING:
        // Nice yellow (not fully saturated) - RGB(200, 180, 0)
        formatted_message = "\033[38;2;200;180;0m[WARNING]\033[0m " + std::string(message);
        break;
    case LOG_TYPE_ERROR:
        // Nice red (not fully saturated) - RGB(200, 80, 80)
        formatted_message = "\033[38;2;200;80;80m[ERROR]\033[0m " + std::string(message);
        break;
    default:
        formatted_message = std::string(message);
        break;
    }

    if (std::this_thread::get_id() == g_main_thread_id)
    {
        printf("%s\n", formatted_message.c_str());
    }
    else
    {
        LogQueue& log_queue = GetLogQueue();
        std::lock_guard lock(log_queue.mutex);
        log_queue.queue.push(formatted_message);
    }
}

static void ProcessQueuedLogMessages()
{
    LogQueue& log_queue = GetLogQueue();
    std::lock_guard lock(log_queue.mutex);
    while (!log_queue.queue.empty())
    {
        std::string message = log_queue.queue.front();
        log_queue.queue.pop();
        printf("%s\n", message.c_str());
    }
}

static void UpdateEditor() {
    UpdateImporter();
    ProcessQueuedLogMessages();
//    UpdateEditorServer();
    UpdateView();
}

void HandleStatsEvents(EventId event_id, const void* event_data)
{
    (void)event_id;
    EditorEventStats* stats = (EditorEventStats*)event_data;
    g_editor.fps = stats->fps;
    g_editor.stats_requested = false;
}

void HandleImported(EventId event_id, const void* event_data) {
    (void)event_id;

    ImportEvent* import_event = (ImportEvent*)event_data;
    AssetLoadedEvent event = { import_event->name, import_event->type };
    Send(EVENT_HOTLOAD, &event);

    AddNotification(NOTIFICATION_TYPE_INFO, "imported '%s'", import_event->name->value);

#if 0
    BroadcastAssetChange(import_event->name, import_event->type);
#endif
}

static void SaveUserConfig(Props* user_config) {
    SaveViewUserConfig(user_config);
    SaveProps(user_config, "./.noz/user.cfg");
}

static void SaveUserConfig() {
    Stream* config_stream = CreateStream(ALLOCATOR_DEFAULT, 8192);
    if (!config_stream)
        return;

    Props* user_config = Props::Load(config_stream);
    if (!user_config)
        user_config = new Props();

    SaveUserConfig(user_config);

    delete user_config;
    Free(config_stream);
}

static void InitUserConfig(Props* user_config) {
    InitViewUserConfig(user_config);
}

static void InitUserConfig() {
    if (Stream* config_stream = LoadStream(nullptr, "./.noz/user.cfg")) {
        if (Props* user_config = Props::Load(config_stream)) {
            InitUserConfig(user_config);
            delete user_config;
        }

        Free(config_stream);
    }
}

static void InitConfig() {
    // Determine config path - use project path if specified, otherwise current directory
    fs::path config_path;
    const char* project_arg = GetArgValue("project");
    if (project_arg) {
        fs::path project_path = project_arg;
        if (project_path.is_relative()) {
            project_path = fs::current_path() / project_path;
        }
        config_path = fs::absolute(project_path) / "editor.cfg";
    } else {
        config_path = "./editor.cfg";
    }

    if (fs::exists(config_path)) {
        g_editor.config_timestamp = fs::last_write_time(config_path);
    }

    if (Stream* config_stream = LoadStream(nullptr, config_path)) {
        g_config = Props::Load(config_stream);
        Free(config_stream);
    }

    if (g_config == nullptr) {
        LogError("missing configuration '%s'", config_path.string().c_str());
        g_config = new Props();
    }

    fs::path project_path = project_arg
        ? fs::absolute(fs::path(project_arg).is_relative() ? fs::current_path() / project_arg : fs::path(project_arg))
        : fs::current_path();

    // Read in the source paths
    for (auto& path : g_config->GetKeys("source")) {
        fs::path full_path = project_path / path;
        full_path = canonical(full_path);
        if (!fs::exists(full_path))
            fs::create_directories(full_path);
        SetValue(g_editor.source_paths[g_editor.source_path_count], full_path.string().c_str());
        Lowercase(g_editor.source_paths[g_editor.source_path_count]);
        g_editor.source_path_count++;
    }

    g_editor.output_path = fs::absolute(project_path / fs::path(g_config->GetString("editor", "output_path", "assets"))).string();
    g_editor.unity_path = fs::absolute(project_path / fs::path(g_config->GetString("editor", "unity_path", "./assets/noz")));
    g_editor.save_dir = g_config->GetString("editor", "save_path", "assets");
    g_editor.unity = g_config->GetBool("editor", "unity", false);
    g_editor.project_path = project_path.string();

    fs::create_directories(g_editor.output_path);
}

static void InitImporters() {
    g_editor.importers = (AssetImporter*)Alloc(ALLOCATOR_DEFAULT, sizeof(AssetImporter) * ASSET_TYPE_COUNT);
    g_editor.importers[ASSET_TYPE_ANIMATED_MESH] = GetAnimatedMeshImporter();
    g_editor.importers[ASSET_TYPE_ANIMATION] = GetAnimationImporter();
    g_editor.importers[ASSET_TYPE_FONT] = GetFontImporter();
    g_editor.importers[ASSET_TYPE_MESH] = GetMeshImporter();
    g_editor.importers[ASSET_TYPE_SHADER] = GetShaderImporter();
    g_editor.importers[ASSET_TYPE_SOUND] = GetSoundImporter();
    g_editor.importers[ASSET_TYPE_TEXTURE] = GetTextureImporter();
    g_editor.importers[ASSET_TYPE_VFX] = GetVfxImporter();
    g_editor.importers[ASSET_TYPE_SKELETON] = GetSkeletonImporter();
    g_editor.importers[ASSET_TYPE_EVENT] = GetEventImporter();

#ifdef _DEBUG
    for (int i=0; i<ASSET_TYPE_COUNT; i++)
    {
        assert(g_editor.importers[i].type == (AssetType)i);
        assert(g_editor.importers[i].import_func);
        assert(g_editor.importers[i].ext);
    }
#endif
}

void InitEditor() {
    g_main_thread_id = std::this_thread::get_id();
    g_editor.asset_allocator = CreatePoolAllocator(sizeof(FatAssetData), MAX_ASSETS);

    InitImporters();
    InitLog(HandleLog);
    Listen(EDITOR_EVENT_STATS, HandleStatsEvents);
    Listen(EDITOR_EVENT_IMPORTED, HandleImported);
}

void ShutdownEditor() {
    SaveUserConfig();
    ShutdownCommandInput();
    ShutdownView();
    //ShutdownEditorServer();
    ShutdownImporter();
}

void EditorHotLoad(const Name* name, AssetType asset_type) {
    HotloadAsset(name, asset_type);
    HotloadEditorAsset( asset_type, name);
}

void BeginTool(const Tool& tool) {
    assert(g_editor.tool.type == TOOL_TYPE_NONE);
    g_editor.tool = tool;

    if (g_editor.tool.input == nullptr)
        g_editor.tool.input = g_view.input_tool;

    PushInputSet(g_editor.tool.input, tool.inherit_input);
}

void EndTool() {
    assert(g_editor.tool.type != TOOL_TYPE_NONE);
    g_editor.tool = {};
    PopInputSet();
    SetSystemCursor(SYSTEM_CURSOR_DEFAULT);
}

void CancelTool() {
    assert(g_editor.tool.type != TOOL_TYPE_NONE);
    if (g_editor.tool.vtable.cancel)
        g_editor.tool.vtable.cancel();

    EndTool();
}

static void InitPalettes() {
    for (auto& palette_key : g_config->GetKeys("palettes")) {
        std::string palette_value = g_config->GetString("palettes", palette_key.c_str(), nullptr);
        Tokenizer tk;
        Init(tk, palette_value.c_str());
        int palette_id = ExpectInt(tk);
        g_editor.palette_map[palette_id] = g_editor.palette_count;
        g_editor.palettes[g_editor.palette_count++] = {
            .name = GetName(palette_key.c_str()),
            .id = palette_id
        };
    }
}

static void ResolveAssetPaths() {
    g_editor.editor_assets_path = fs::absolute(fs::current_path() / "assets").string();
    g_editor.asset_paths[g_editor.asset_path_count++] = g_editor.output_path.c_str();
    g_editor.asset_paths[g_editor.asset_path_count++] = g_editor.editor_assets_path.c_str();
    g_editor.asset_paths[g_editor.asset_path_count] = nullptr;
}

void Main() {
    g_main_thread_id = std::this_thread::get_id();

    InitConfig();
    ResolveAssetPaths();

    ApplicationTraits traits = {};
    Init(traits);
    traits.title = "NoZ Editor";
    traits.asset_paths = g_editor.asset_paths;
    traits.load_assets = LoadAssets;
    traits.unload_assets = UnloadAssets;
    traits.hotload_asset = EditorHotLoad;
    traits.renderer.msaa = true;
    traits.scratch_memory_size = noz::MB * 128;
    traits.update = UpdateEditor;
    traits.shutdown = ShutdownEditor;

    InitApplication(&traits);
    InitPalettes();

    InitEditor();
    InitLog(HandleLog);
    InitAssetData();
    LoadAssetData();

    InitNotifications();
    InitImporter();
    InitWindow();
    PostLoadAssetData();

    MESH = g_editor.meshes;
    MESH_COUNT = MAX_ASSETS;

    InitView();
    InitCommandInput();
    InitUserConfig();
    //InitEditorServer(g_config);
}

