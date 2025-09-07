//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

// https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences

#ifdef _WIN32

// Need Windows user definitions for VK codes in this file
#undef NOUSER
#include "screen.h"
#include "terminal.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

extern void InitScreen(i32 width, i32 height);
extern void UpdateScreenSize(i32 width, i32 height);
extern ScreenOutputBuffer RenderScreen();

static HANDLE g_console_input = INVALID_HANDLE_VALUE;
static HANDLE g_console_output = INVALID_HANDLE_VALUE;


static int g_screen_width = 0;
static int g_screen_height = 0;
static int g_cursor_x = 0;
static int g_cursor_y = 0;

// Output buffer for batched VT sequences
static std::string g_output_buffer;

static TerminalRenderCallback g_render_callback = nullptr;
static TerminalResizeCallback g_resize_callback = nullptr;
static std::atomic<bool> g_needs_redraw = false;

// VT sequence color codes for different terminal colors
static const char* g_color_sequences[] = {
    "\033[0m",     // Default (reset)
    "\033[30;47m", // STATUS_BAR (black on white)
    "\033[37;40m", // COMMAND_LINE (white on black)
    "\033[92m",    // SUCCESS (bright green)
    "\033[91m",    // ERROR (bright red)
    "\033[93m"     // WARNING (bright yellow)
};

static int g_current_color = 0;

void SetRenderCallback(TerminalRenderCallback callback)
{
    g_render_callback = callback;
}

void SetResizeCallback(TerminalResizeCallback callback)
{
    g_resize_callback = callback;
}

void UpdateScreenSize()
{
    CONSOLE_SCREEN_BUFFER_INFO csbi = {};
    if (!GetConsoleScreenBufferInfo(g_console_output, &csbi))
        return;

    int new_width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    int new_height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

    static int debug_counter = 0;
    debug_counter++;

    if (new_width != g_screen_width || new_height != g_screen_height)
    {
        // Update to new dimensions
        g_screen_width = new_width;
        g_screen_height = new_height;

        // Set buffer size to match window size (removes scrollbar)
        COORD buffer_size = {(SHORT)new_width, (SHORT)new_height};
        SetConsoleScreenBufferSize(g_console_output, buffer_size);

        UpdateScreenSize(new_width, new_height);

        if (g_resize_callback)
            g_resize_callback(g_screen_width, g_screen_height);

        // Force immediate redraw after resize
        RequestRender();
        RenderTerminal();
    }
}

void RenderTerminal()
{
    if (!g_render_callback)
        return;

    g_render_callback(GetScreenWidth(), GetScreenHeight());

    auto buffer = RenderScreen();
    if (buffer.size == 0)
        return;

    DWORD written;
    WriteConsoleA(g_console_output, buffer.buffer, (DWORD)buffer.size, &written, nullptr);
    FlushFileBuffers(g_console_output);
    g_needs_redraw = false;
}

void RequestRender()
{
    g_needs_redraw = true;
}

int GetTerminalKey()
{
    INPUT_RECORD input_record;
    DWORD event_count;

    // Use PeekConsoleInput to safely check for events without consuming them
    if (!PeekConsoleInput(g_console_input, &input_record, 1, &event_count) || event_count == 0)
        return ERR; // No input available

    // Now we know there's at least one event, so ReadConsoleInput won't block
    while (event_count > 0 && ReadConsoleInput(g_console_input, &input_record, 1, &event_count))
    {
        if (input_record.EventType == KEY_EVENT && input_record.Event.KeyEvent.bKeyDown)
        {
            // Handle special keys first
            WORD vk = input_record.Event.KeyEvent.wVirtualKeyCode;
            char ch = input_record.Event.KeyEvent.uChar.AsciiChar;

            switch (vk)
            {
            case VK_ESCAPE:
                return 27; // ESC
            case VK_LEFT:
                return KEY_LEFT;
            case VK_RIGHT:
                return KEY_RIGHT;
            case VK_UP:
                return KEY_UP;
            case VK_DOWN:
                return KEY_DOWN;
            case VK_HOME:
                return KEY_HOME;
            case VK_END:
                return KEY_END;
            case VK_PRIOR:
                return KEY_PPAGE;
            case VK_NEXT:
                return KEY_NPAGE;
            }

            // Return the ASCII character for regular keys
            if (ch != 0)
                return ch;
        }
        else if (input_record.EventType == MOUSE_EVENT)
        {
            // Handle mouse events (including scroll wheel)
            MOUSE_EVENT_RECORD& mouse = input_record.Event.MouseEvent;
            if (mouse.dwEventFlags == MOUSE_WHEELED)
                return KEY_MOUSE; // Return special mouse key
        }
        else if (input_record.EventType == WINDOW_BUFFER_SIZE_EVENT)
        {
            // Handle resize events immediately
            UpdateScreenSize();
        }

        event_count--;
    }

    return ERR; // No input available
}


void AddChar(char ch, int count)
{
    for (int i=0; i<count; i++)
        AddChar(ch);
}

static void AddEscapedChar(char ch)
{
    g_output_buffer += ch;
}

void AddEscapedString(const char* str)
{
    while (*str)
        AddEscapedChar(*str++);
}

void AddChar(char ch)
{
    // Only add character if within screen bounds
    if (g_cursor_y >= 0 && g_cursor_y < g_screen_height && g_cursor_x >= 0 && g_cursor_x < g_screen_width)
        g_output_buffer += ch;

    // Advance cursor position tracking
    g_cursor_x++;
    if (g_cursor_x >= g_screen_width)
    {
        g_cursor_x = 0;
        g_cursor_y++;
    }
}

void AddString(const char* str)
{
    while (*str)
        AddChar(*str++);
}

void AddString(const TString& tstr, int cursor_pos, int truncate)
{
#if 0
    size_t pos = 0;
    size_t visual_length = 0;
    const std::string& str = tstr.formatted;
    size_t str_len = str.size();
    while (pos < str_len && visual_length < truncate)
    {
        // Skip and add colors
        if (str[pos] == '\033')
        {
            while (pos < str.length() && str[pos] != 'm')
                AddEscapedChar(str[pos++]);
            if (pos < str.length())
                AddEscapedChar(str[pos++]);

            continue;
        }

        bool cursor_here = cursor_pos == visual_length;
        if (cursor_here)
            BeginInverse();

        AddChar(str[pos++]);
        visual_length++;

        if (cursor_here)
            EndInverse();
    }
#endif
}

void SetColor(int pair)
{
    if (pair >= 0 && pair < (int)(sizeof(g_color_sequences) / sizeof(g_color_sequences[0])))
    {
        if (pair != g_current_color)
        {
            g_output_buffer += g_color_sequences[pair];
            g_current_color = pair;
        }
    }
}

void UnsetColor(int pair)
{
    // Reset to default color (index 0)
    if (g_current_color != 0)
    {
        g_output_buffer += g_color_sequences[0];
        g_current_color = 0;
    }
}

void SetColor256(int fg, int bg)
{
    std::string color_seq = "\033[";

    if (fg >= 0 && fg <= 255)
    {
        color_seq += "38;5;" + std::to_string(fg);
        if (bg >= 0 && bg <= 255)
        {
            color_seq += ";48;5;" + std::to_string(bg);
        }
    }
    else if (bg >= 0 && bg <= 255)
    {
        color_seq += "48;5;" + std::to_string(bg);
    }

    color_seq += "m";
    g_output_buffer += color_seq;
}

void SetColorRGB(int r, int g, int b, int bg_r, int bg_g, int bg_b)
{
    std::string color_seq = "\033[";

    // Clamp RGB values to 0-255
    r = std::max(0, std::min(255, r));
    g = std::max(0, std::min(255, g));
    b = std::max(0, std::min(255, b));

    // Set foreground RGB color
    color_seq += "38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b);

    // Set background RGB color if provided
    if (bg_r >= 0 && bg_g >= 0 && bg_b >= 0)
    {
        bg_r = std::max(0, std::min(255, bg_r));
        bg_g = std::max(0, std::min(255, bg_g));
        bg_b = std::max(0, std::min(255, bg_b));
        color_seq += ";48;2;" + std::to_string(bg_r) + ";" + std::to_string(bg_g) + ";" + std::to_string(bg_b);
    }

    color_seq += "m";
    g_output_buffer += color_seq;
}

void BeginColor(int r, int g, int b)
{
    // Clamp RGB values to 0-255
    r = std::max(0, std::min(255, r));
    g = std::max(0, std::min(255, g));
    b = std::max(0, std::min(255, b));
    
    std::string color_seq = "\033[38;2;" + std::to_string(r) + ";" + std::to_string(g) + ";" + std::to_string(b) + "m";
    g_output_buffer += color_seq;
}

void EndColor()
{
    g_output_buffer += "\033[0m";
}

void SetBold(bool enabled)
{
    if (enabled)
        AddEscapedString("\033[1m");
    else
        AddEscapedString("\033[22m");
}

void SetUnderline(bool enabled)
{
    if (enabled)
    {
        g_output_buffer += "\033[4m";
    }
    else
    {
        g_output_buffer += "\033[24m"; // Reset underline
    }
}

void SetItalic(bool enabled)
{
    if (enabled)
    {
        g_output_buffer += "\033[3m";
    }
    else
    {
        g_output_buffer += "\033[23m"; // Reset italic
    }
}

void SetScrollRegion(int top, int bottom)
{
    // VT sequence: ESC[top;bottomr sets scroll region (1-based)
    // Clamp values to valid screen range
    top = std::max(1, std::min(g_screen_height, top));
    bottom = std::max(top, std::min(g_screen_height, bottom));

    g_output_buffer += "\033[" + std::to_string(top) + ";" + std::to_string(bottom) + "r";
}

void ResetScrollRegion()
{
    // VT sequence: ESC[r resets scroll region to full screen
    g_output_buffer += "\033[r";
}

void SetCursorVisible(bool visible)
{
    // Use VT sequence to show/hide cursor: \033[?25h (show) or \033[?25l (hide)
    if (visible)
    {
        g_output_buffer += "\033[?25h";
    }
    else
    {
        g_output_buffer += "\033[?25l";
    }
}

bool HasColorSupport()
{
    return true; // Windows console always supports colors
}

void UpdateTerminal()
{
    // Check for resize events by polling window size
    UpdateScreenSize();
}

int GetCursorX()
{
    return g_cursor_x;
}

void BeginInverse()
{
    AddEscapedString("\033[7m");
}

void EndInverse()
{
    AddEscapedString("\033[27m");
}

void InitTerminal()
{
    // Get console handles
    g_console_input = GetStdHandle(STD_INPUT_HANDLE);
    g_console_output = GetStdHandle(STD_OUTPUT_HANDLE);

    if (g_console_input == INVALID_HANDLE_VALUE || g_console_output == INVALID_HANDLE_VALUE)
        return;

    // Set input mode to capture mouse, window, and key events
    DWORD input_mode;
    GetConsoleMode(g_console_input, &input_mode);

    // Disable line input, echo, and processed input to get raw key events
    input_mode &= ~ENABLE_LINE_INPUT;
    input_mode &= ~ENABLE_ECHO_INPUT;
    input_mode &= ~ENABLE_PROCESSED_INPUT;
    input_mode &= ~ENABLE_AUTO_POSITION;
    // Disable VT input to get virtual key codes instead of escape sequences
    input_mode &= ~ENABLE_VIRTUAL_TERMINAL_INPUT;
    // Disable quick edit to prevent interfering with key capture
    input_mode &= ~ENABLE_QUICK_EDIT_MODE;

    // Enable mouse and window input
    input_mode |= ENABLE_MOUSE_INPUT;
    input_mode |= ENABLE_WINDOW_INPUT;
    input_mode |= ENABLE_EXTENDED_FLAGS;

    SetConsoleMode(g_console_input, input_mode);

    // Set output mode - this is the key change for VT sequences
    DWORD output_mode;
    GetConsoleMode(g_console_output, &output_mode);
    output_mode |= ENABLE_PROCESSED_OUTPUT;
    output_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING; // Enable VT sequence processing
    output_mode &= ~ENABLE_WRAP_AT_EOL_OUTPUT;
    SetConsoleMode(g_console_output, output_mode);

    // Verify VT processing is actually enabled
    DWORD final_mode;
    GetConsoleMode(g_console_output, &final_mode);

    CONSOLE_SCREEN_BUFFER_INFO csbi = {};
    if (!GetConsoleScreenBufferInfo(g_console_output, &csbi))
        return;

    InitScreen(
        csbi.srWindow.Right - csbi.srWindow.Left + 1,
        csbi.srWindow.Bottom - csbi.srWindow.Top + 1);

    // Initialize screen size
    UpdateScreenSize();

    // Initialize output buffer and cursor position
    g_output_buffer.reserve(g_screen_width * g_screen_height * 2); // Reserve space for efficiency
    g_cursor_x = 0;
    g_cursor_y = 0;
    g_current_color = 0;

    // Switch to alternate screen buffer and hide cursor
    g_output_buffer.clear();
    g_output_buffer += "\033]0;NoZ Editor\007"; // Set window title
    g_output_buffer += "\033[?1049h"; // Enable alternate screen buffer
    g_output_buffer += "\033[2J";     // Clear screen
    g_output_buffer += "\033[H";      // Move to home position
    g_output_buffer += "\033[?25l";   // Hide cursor

    // Write initial setup sequences
    DWORD written;
    WriteConsoleA(g_console_output, g_output_buffer.c_str(), (DWORD)g_output_buffer.length(), &written, nullptr);

    g_output_buffer.clear();
}

void ShutdownTerminal()
{
    // Reset to default colors, show cursor, and exit alternate screen
    g_output_buffer.clear();
    g_output_buffer += "\033[0m";     // Reset all attributes
    g_output_buffer += "\033[?25h";   // Show cursor
    g_output_buffer += "\033[?1049l"; // Disable alternate screen buffer (restore original screen)

    // Write final reset sequences
    if (!g_output_buffer.empty())
    {
        DWORD written;
        WriteConsoleA(g_console_output, g_output_buffer.c_str(), (DWORD)g_output_buffer.length(), &written, nullptr);
    }

    // Reset console modes to default
    if (g_console_input != INVALID_HANDLE_VALUE)
    {
        DWORD input_mode;
        GetConsoleMode(g_console_input, &input_mode);
        input_mode |= ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT;
        input_mode &= ~(ENABLE_MOUSE_INPUT | ENABLE_WINDOW_INPUT);
        SetConsoleMode(g_console_input, input_mode);
    }
}

#endif // _WIN32