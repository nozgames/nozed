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

static void UpdateEditor()
{
    ProcessQueuedLogMessages();
    UpdateEditorServer();
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

    BroadcastAssetChange(import_event->name, import_event->type);
}

static void SaveUserConfig(Props* user_config)
{
    SaveViewUserConfig(user_config);
    SaveProps(user_config, "./.noz/user.cfg");
}

static void SaveUserConfig()
{
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

static void InitUserConfig(Props* user_config)
{
    InitViewUserConfig(user_config);
}

static void InitUserConfig()
{
    if (Stream* config_stream = LoadStream(nullptr, "./.noz/user.cfg"))
    {
        if (Props* user_config = Props::Load(config_stream))
        {
            InitUserConfig(user_config);
            delete user_config;
        }

        Free(config_stream);
    }
}

static void InitConfig() {
    std::filesystem::path config_path = "./editor.cfg";

    g_editor.config_timestamp = std::filesystem::last_write_time(config_path);

    if (Stream* config_stream = LoadStream(nullptr, config_path)) {
        g_config = Props::Load(config_stream);
        Free(config_stream);
    }

    if (g_config == nullptr) {
        LogError("missing configuration '%s'", config_path.string().c_str());
        g_config = new Props();
    }

    // Read in the source paths
    for (auto& path : g_config->GetKeys("source")) {
        std::filesystem::path full_path = std::filesystem::current_path() / path;
        full_path = canonical(full_path);
        Copy(g_editor.asset_paths[g_editor.asset_path_count++], 4096, full_path.string().c_str());
    }

    g_editor.output_dir = fs::absolute(fs::path(g_config->GetString("output", "directory", "assets")));

    fs::create_directories(g_editor.output_dir);
}

static void InitImporters() {
    g_editor.importers = (AssetImporter*)Alloc(ALLOCATOR_DEFAULT, sizeof(AssetImporter) * ASSET_TYPE_COUNT);
    g_editor.importers[ASSET_TYPE_ANIMATION] = GetAnimationImporter();
    g_editor.importers[ASSET_TYPE_FONT] = GetFontImporter();
    g_editor.importers[ASSET_TYPE_MESH] = GetMeshImporter();
    g_editor.importers[ASSET_TYPE_SHADER] = GetShaderImporter();
    g_editor.importers[ASSET_TYPE_SOUND] = GetSoundImporter();
    g_editor.importers[ASSET_TYPE_TEXTURE] = GetTextureImporter();
    g_editor.importers[ASSET_TYPE_VFX] = GetVfxImporter();
    g_editor.importers[ASSET_TYPE_SKELETON] = GetSkeletonImporter();

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
    ShutdownEditorServer();
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
    SetCursor(SYSTEM_CURSOR_DEFAULT);
}

void CancelTool() {
    assert(g_editor.tool.type != TOOL_TYPE_NONE);
    if (g_editor.tool.vtable.cancel)
        g_editor.tool.vtable.cancel();

    EndTool();
}

int main(int argc, const char* argv[]) {
    g_editor.exe = argv[0];

    g_main_thread_id = std::this_thread::get_id();

    InitConfig();

    ApplicationTraits traits = {};
    Init(traits);
    traits.title = "NoZ Editor";
    traits.assets_path = "build/assets";
    traits.load_assets = LoadAssets;
    traits.unload_assets = UnloadAssets;
    traits.hotload_asset = EditorHotLoad;
    traits.scratch_memory_size = noz::MB * 128;

    InitApplication(&traits, argc, argv);
    InitEditor();
    InitLog(HandleLog);

    InitAssetData();
    LoadAssetData();
    InitNotifications();
    InitImporter();
    InitWindow();
    PostLoadAssetData();
    InitView();
    InitCommandInput();
    InitUserConfig();
    InitEditorServer(g_config);

    while (UpdateApplication()) {
        UpdateImporter();
        UpdateEditor();
        UpdateView();
    }

    SaveUserConfig();
    ShutdownCommandInput();

    if (IsWindowCreated())
        ShutdownView();

    ShutdownEditor();
    ShutdownApplication();

    return 0;
}
