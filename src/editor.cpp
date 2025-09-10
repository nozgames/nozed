//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "editor.h"
#include "asset_editor/asset_editor.h"
#include "editor_assets.h"
#include "server.h"
#include "tui/screen.h"
#include "tui/terminal.h"
#include "tui/text_input.h"
#include "views/views.h"


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

View* GetView()
{
    if (g_editor.view_stack_count == 0)
        return g_editor.log_view;

    return g_editor.view_stack[g_editor.view_stack_count-1];
}

void PushView(View* view)
{
    assert(view);
    assert(g_editor.view_stack_count < MAX_VIEWS);
    g_editor.view_stack[g_editor.view_stack_count++] = view;
}

void PopView()
{
    assert(g_editor.view_stack_count > 0);
    auto current = g_editor.view_stack[g_editor.view_stack_count-1];
    // if (!current->CanPopFromStack())
    //     return;

    g_editor.view_stack_count--;
}

// Thread safety for log messages
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
        AddMessage(g_editor.log_view, formatted_message.c_str());
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
        AddMessage(g_editor.log_view, message.c_str());
    }
}

static void DrawStatusBar(const RectInt& rect)
{
    static const char* title = "NoZ Editor";
    static const char* cmd_mode = " - Command Mode";
    static const char* fps = "FPS: ";

    auto line = GetBottom(rect) - 2;
    auto eol = GetRight(rect);
    WriteScreen(rect.x, line, title, TCOLOR_BLACK);

    if (g_editor.command_mode)
        WriteScreen(cmd_mode, TCOLOR_BLACK);

    if (HasConnectedClient())
    {
        if (!g_editor.stats_requested)
        {
            RequestStats();
            g_editor.stats_requested = true;
        }

        char fps_value[32];
        sprintf(fps_value, "%d", Min(99999, g_editor.fps));
        WriteScreen(eol - 10, line, fps, TCOLOR_BLACK);
        WriteScreen(eol - 5, line, fps_value, TCOLOR_BLACK);
    }
    else
        g_editor.stats_requested = false;

    WriteBackgroundColor({rect.x, line, rect.width, 1}, TCOLOR_BACKGROUND_WHITE);
}

static void DrawCommandLine(const RectInt& rect)
{
    if (g_editor.search_mode)
    {
        WriteScreen(rect.x, rect.y, "/");
        Render(g_editor.search_input);
    }
    else if (g_editor.command_mode)
    {
        WriteScreen(rect.x, rect.y, ":");
        Render(g_editor.command_input);
    }
#if 0
    else
    {
        // Check if there's an active search to display
        std::string search_text = GetText(g_editor.search_input);
        if (!search_text.empty())
        {
            AddChar('/');
            AddString(search_text.c_str());
            AddString(" (filtered - ESC to clear)");
        }
        
        // Fill remaining space
        int used_chars = GetCursorX();
        for (int i = used_chars; i < width; i++)
            AddChar(' ');
    }
#endif
}

static void UpdateEditor()
{
    // Process any queued log messages from background threads
    ProcessQueuedLogMessages();

    UpdateEditorServer();
    UpdateTerminal();

    int key = GetTerminalKey();
    if (key == ERR)
    {
        RenderTerminal();
        std::this_thread::yield();
        return;
    }

    // Handle mouse events (including scroll) to prevent terminal scrolling
    if (key == KEY_MOUSE)
    {
        // Just consume the mouse event to prevent terminal scrolling
        // TODO: Implement proper mouse/scroll handling later
        return; // Don't process mouse events as regular keys
    }

    if (g_editor.search_mode)
    {
        if (key == '\n' || key == '\r')
        {
            // Finish search input but keep the filtered view
            g_editor.search_mode = false;
            SetActive(g_editor.search_input, false);

            // Show cursor in current view when exiting search mode
            // View* view = GetView();
            // view->SetCursorVisible(true);
        }
        else if (key == 27)
        { // Escape
            // Cancel search and return to full unfiltered view
            g_editor.search_mode = false;
            SetActive(g_editor.search_input, false);
            Clear(g_editor.search_input);

            // IView* current_view = GetView();
            // current_view->ClearSearch();
            // current_view->SetCursorVisible(true);
        }
        else
        {
            HandleKey(g_editor.search_input, key);

            // Update search in real-time
            // std::string pattern = GetText(g_editor.search_input);
            // IView* current_view = GetView();
            // current_view->SetSearchPattern(pattern);
        }
    }
    else if (g_editor.command_mode)
    {
        if (key == '\n' || key == '\r')
        {
            std::string command = GetText(g_editor.command_input);
            HandleCommand(command);
            g_editor.command_mode = false;
            SetActive(g_editor.command_input, false);
            Clear(g_editor.command_input);

            // Show cursor in current view when exiting command mode
            // IView* current_view = GetView();
            // current_view->SetCursorVisible(true);
        }
        else if (key == 27)
        { // Escape
            g_editor.command_mode = false;
            SetActive(g_editor.command_input, false);
            Clear(g_editor.command_input);

            // Show cursor in current view when exiting command mode
            // IView* current_view = GetView();
            // current_view->SetCursorVisible(true);
        }
        else
        {
            HandleKey(g_editor.command_input, key);
        }
    }
    else
    {
        View* current_view = GetView();

        if (key == '/')
        {
        }
        else if (key == ':')
        {
            g_editor.command_mode = true;
            SetActive(g_editor.command_input, true);
            Clear(g_editor.command_input);

            // Hide cursor in current view when entering command mode
//                current_view->SetCursorVisible(false);
        }
        else if (key == 'q')
        {
            g_editor.is_running = false;
        }
        else if (key == 27)  // Escape - clear search or pop view from stack
        {
            // Check if there's an active search to clear
            std::string search_text = GetText(g_editor.search_input);
            if (!search_text.empty())
            {
                // Clear active search
                Clear(g_editor.search_input);
                // View* view = GetView();
                // current_view->ClearSearch();
            }
            else
            {
                // No search to clear, pop view from stack
                PopView();
            }
        }
        else
        {
            // Route input to current view
            // if (!current_view->HandleKey(key))
            // {
            //     // View didn't handle the key, could add default handling here
            // }
        }
    }

    RenderTerminal();
}

void RenderEditor(const RectInt& rect)
{
    ClearScreen(TCHAR_NONE);
    DrawStatusBar(rect);
    DrawCommandLine({rect.x, rect.height - 1, rect.width, 1});

    auto view = GetView();
    if (view != nullptr && view->traits->render)
        view->traits->render(view, {rect.x, rect.y, rect.width, rect.height - 2});
}

void HandleStatsEvents(EventId event, const void* event_data)
{
    EditorEventStats* stats = (EditorEventStats*)event_data;
    g_editor.fps = stats->fps;
    g_editor.stats_requested = false;
}

static void InitConfig()
{
    std::filesystem::path config_path = "./editor.cfg";
    if (Stream* config_stream = LoadStream(nullptr, config_path))
    {
        g_config = Props::Load(config_stream);
        Free(config_stream);

        if (g_config != nullptr)
            return;
    }

    g_config = new Props();

    LogError("missing configuration '%s'", config_path.string().c_str());
}

void InitEditor()
{
    g_scratch_allocator = CreateArenaAllocator(32 * noz::MB, "scratch");
    g_main_thread_id = std::this_thread::get_id();

    ApplicationTraits traits = {};
    Init(traits);

    InitEvent(&traits);
    InitLog(HandleLog);
    InitTerminal();
    SetRenderCallback([](int width, int height) { RenderEditor({0, 0, width, height}); });
    int term_height = GetScreenHeight();
    int term_width = GetScreenWidth();
    g_editor.log_view = CreateLogView(ALLOCATOR_DEFAULT);
    g_editor.command_input = CreateTextInput(1, term_height - 1, term_width - 1);
    g_editor.search_input = CreateTextInput(1, term_height - 1, term_width - 1);
    g_editor.is_running = true;

    InitEditorServer(g_config);

    Listen(EDITOR_EVENT_STATS, HandleStatsEvents);
}

void ShutdownEditor()
{
    ShutdownEditorServer();
    Destroy(g_editor.command_input);
    Destroy(g_editor.search_input);
    ShutdownImporter();
    ShutdownTerminal();
}

int main(int argc, const char* argv[])
{
    g_editor.exe = argv[0];

    g_main_thread_id = std::this_thread::get_id();

    InitConfig();
    InitImporter();

    ApplicationTraits traits = {};
    Init(traits);
    traits.assets_path = "build/assets";
    traits.console = true;
    traits.load_assets = LoadAssets;
    traits.unload_assets = UnloadAssets;

    InitApplication(&traits, argc, argv);
    InitEditor();
    InitLog(HandleLog);

    for (int i = 1; i < argc; i++)
    {
        HandleCommand(argv[i]);

        // IF a quit was specified then it means to auto-quit after processing commands
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
