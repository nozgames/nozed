//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

constexpr int MAX_VIEWS = 16;

struct LogView;
struct View;
struct TextInputBox;

struct Editor
{
    LogView* log_view;
    View* view_stack[MAX_VIEWS];
    u32 view_stack_count = 0;
    TextInputBox* command_input;
    TextInputBox* search_input;
    bool command_mode;
    bool search_mode;
    bool is_running;
    bool auto_quit;
    int fps;
    bool stats_requested;
    const char* exe;
};

extern Editor g_editor;

// @editor
extern View* GetView();
extern void PushView(View* view);
extern void PopView();

// @shortcut
