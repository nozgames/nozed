//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

// https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences

#ifdef _WIN32

#undef NOUSER
#include "screen.h"
#include "terminal.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

extern void InitScreen(i32 width, i32 height);
extern void UpdateScreenSize(i32 width, i32 height);
extern ScreenOutputBuffer RenderScreen();

struct WindowsTerminal
{
    Vec2Int screen_size;
    Vec2Int cursor;
    std::string g_output_buffer;
    HANDLE input;
    DWORD input_mode;
    HANDLE output;
    DWORD output_mode;
    TerminalRenderCallback render_callback;
    TerminalResizeCallback resize_callback;
};

static WindowsTerminal g_terminal = {};

void SetRenderCallback(TerminalRenderCallback callback)
{
    g_terminal.render_callback = callback;
}

void SetResizeCallback(TerminalResizeCallback callback)
{
    g_terminal.resize_callback = callback;
}

void UpdateScreenSize()
{
    CONSOLE_SCREEN_BUFFER_INFO csbi = {};
    if (!GetConsoleScreenBufferInfo(g_terminal.output, &csbi))
        return;

    int new_width = csbi.srWindow.Right - csbi.srWindow.Left + 1;
    int new_height = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;

    static int debug_counter = 0;
    debug_counter++;

    if (new_width != g_terminal.screen_size.x || new_height != g_terminal.screen_size.y)
    {
        // Update to new dimensions
        g_terminal.screen_size.x = new_width;
        g_terminal.screen_size.y = new_height;

        // Set buffer size to match window size (removes scrollbar)
        COORD buffer_size = {(SHORT)new_width, (SHORT)new_height};
        SetConsoleScreenBufferSize(g_terminal.output, buffer_size);

        UpdateScreenSize(new_width, new_height);

        if (g_terminal.resize_callback)
            g_terminal.resize_callback(g_terminal.screen_size.x, g_terminal.screen_size.y);

        // Force immediate redraw after resize
        RenderTerminal();
    }
}

void RenderTerminal()
{
    if (!g_terminal.render_callback)
        return;

    g_terminal.render_callback(GetScreenWidth(), GetScreenHeight());

    auto buffer = RenderScreen();
    if (buffer.size == 0)
        return;

    DWORD written;
    WriteConsoleA(g_terminal.output, buffer.buffer, (DWORD)buffer.size, &written, nullptr);
    FlushFileBuffers(g_terminal.output);
}

int GetTerminalKey()
{
    INPUT_RECORD input_record;
    DWORD event_count;

    if (!PeekConsoleInput(g_terminal.input, &input_record, 1, &event_count) || event_count == 0)
        return ERR;

    while (event_count > 0 && ReadConsoleInput(g_terminal.input, &input_record, 1, &event_count))
    {
        if (input_record.EventType == KEY_EVENT && input_record.Event.KeyEvent.bKeyDown)
        {
            // Handle special keys first
            WORD vk = input_record.Event.KeyEvent.wVirtualKeyCode;
            char ch = input_record.Event.KeyEvent.uChar.AsciiChar;

            switch (vk)
            {
            case VK_ESCAPE:
                return 27;
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

            if (ch != 0)
                return ch;
        }
        else if (input_record.EventType == MOUSE_EVENT)
        {
            MOUSE_EVENT_RECORD& mouse = input_record.Event.MouseEvent;
            if (mouse.dwEventFlags == MOUSE_WHEELED)
                return KEY_MOUSE; // Return special mouse key
        }
        else if (input_record.EventType == WINDOW_BUFFER_SIZE_EVENT)
        {
            UpdateScreenSize();
        }

        event_count--;
    }

    return ERR;
}

void UpdateTerminal()
{
    UpdateScreenSize();
}

void InitTerminal()
{
    g_terminal.input = GetStdHandle(STD_INPUT_HANDLE);
    g_terminal.output = GetStdHandle(STD_OUTPUT_HANDLE);

    if (g_terminal.input == INVALID_HANDLE_VALUE ||
        g_terminal.output == INVALID_HANDLE_VALUE)
        return;

    DWORD input_mode;
    GetConsoleMode(g_terminal.input, &input_mode);
    g_terminal.input_mode = input_mode;
    input_mode &= ~ENABLE_LINE_INPUT;
    input_mode &= ~ENABLE_ECHO_INPUT;
    input_mode &= ~ENABLE_PROCESSED_INPUT;
    input_mode &= ~ENABLE_AUTO_POSITION;
    input_mode &= ~ENABLE_VIRTUAL_TERMINAL_INPUT;
    input_mode &= ~ENABLE_QUICK_EDIT_MODE;
    input_mode |= ENABLE_MOUSE_INPUT;
    input_mode |= ENABLE_WINDOW_INPUT;
    input_mode |= ENABLE_EXTENDED_FLAGS;
    SetConsoleMode(g_terminal.input, input_mode);

    DWORD output_mode;
    GetConsoleMode(g_terminal.output, &output_mode);
    g_terminal.output_mode = output_mode;
    output_mode |= ENABLE_PROCESSED_OUTPUT;
    output_mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    output_mode &= ~ENABLE_WRAP_AT_EOL_OUTPUT;
    SetConsoleMode(g_terminal.output, output_mode);

    CONSOLE_SCREEN_BUFFER_INFO csbi = {};
    if (!GetConsoleScreenBufferInfo(g_terminal.output, &csbi))
        return;

    InitScreen(
        csbi.srWindow.Right - csbi.srWindow.Left + 1,
        csbi.srWindow.Bottom - csbi.srWindow.Top + 1);

    UpdateScreenSize();
}

void ShutdownTerminal()
{
    if (g_terminal.input == INVALID_HANDLE_VALUE)
        return;

    SetConsoleMode(g_terminal.input, g_terminal.input_mode);
    SetConsoleMode(g_terminal.output, g_terminal.output_mode);
}

#endif // _WIN32
