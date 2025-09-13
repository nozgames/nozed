//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "editor.h"
#include <view.h>
#include "editor_assets.h"

extern void InitEvent(ApplicationTraits* traits);
extern void RequestStats();
extern void InitEditorServer(Props* config);
extern void UpdateEditorServer();
extern void ShutdownEditorServer();
extern void HandleCommand(const std::string& str);

void InitImporter();
void ShutdownImporter();

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

void HandleStatsEvents(EventId event, const void* event_data)
{
    EditorEventStats* stats = (EditorEventStats*)event_data;
    g_editor.fps = stats->fps;
    g_editor.stats_requested = false;
}

void HandleImported(EventId event_id, const void* event_data)
{
    HotloadEvent event = { (const char*)event_data };
    Send(EVENT_HOTLOAD, &event);
}

static void InitConfig()
{
    std::filesystem::path config_path = "./editor.cfg";
    if (Stream* config_stream = LoadStream(nullptr, config_path))
    {
        g_config = Props::Load(config_stream);
        Free(config_stream);
}

    if (g_config == nullptr)
    {
        LogError("missing configuration '%s'", config_path.string().c_str());
        g_config = new Props();
    }

    // Read in the source paths
    for (auto& path : g_config->GetKeys("source"))
        Copy(g_editor.asset_paths[g_editor.asset_path_count++], 4096, path.c_str());

}

void InitEditor()
{
    g_main_thread_id = std::this_thread::get_id();

    InitLog(HandleLog);
    InitImporter();
    InitEditorServer(g_config);
    Listen(EDITOR_EVENT_STATS, HandleStatsEvents);
    Listen(EDITOR_EVENT_IMPORTED, HandleImported);
}

void ShutdownEditor()
{
    ShutdownEditorServer();
    ShutdownImporter();
}

void EditorHotLoad(const Name* name)
{
    HotloadAsset(name);
    HotloadEditorAsset(name);
}

int main(int argc, const char* argv[])
{
    g_editor.exe = argv[0];

    g_main_thread_id = std::this_thread::get_id();

    InitConfig();

    ApplicationTraits traits = {};
    Init(traits);
    traits.assets_path = "build/assets";
    traits.console = true;
    traits.load_assets = LoadAssets;
    traits.unload_assets = UnloadAssets;
    traits.hotload_asset = EditorHotLoad;

    InitApplication(&traits, argc, argv);
    InitEditor();
    InitLog(HandleLog);

    for (int i = 1; i < argc; i++)
    {
        HandleCommand(argv[i]);

        if (!g_editor.is_running)
        {
            g_editor.is_running = true;
            g_editor.auto_quit = true;
        }
    }

    bool had_window = IsWindowCreated();
    while (UpdateApplication() && g_editor.is_running)
    {
        if (had_window && !IsWindowCreated() && g_editor.auto_quit)
            break;

        UpdateEditor();

        if (IsWindowCreated())
            UpdateAssetEditor();
        else
            ThreadSleep(1);
    }

    if (IsWindowCreated())
        ShutdownAssetEditor();

    ShutdownEditor();
    ShutdownApplication();

    return 0;
}
