//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "views.h"
#include "../tokenizer.h"

constexpr int MAX_LOG_MESSAGES = 1024;
constexpr int MAX_LOG_MESSAGE_LENGTH = 1024;

struct LogMessage
{
    TChar value[MAX_LOG_MESSAGE_LENGTH];
    u32 length;
};

void AddMessage(LogView* view, const char* str)
{
    Tokenizer tok;
    Init(tok, str);
    
    Token line_token = {};
    
    while (ReadLine(tok, &line_token))
    {
        // Add each line as a separate log message
        if (IsFull(view->messages))
            PopFront(view->messages);

        LogMessage* item = (LogMessage*)PushBack(view->messages);
        
        // Copy the line content (null-terminate the token value)
        char temp_buffer[MAX_LOG_MESSAGE_LENGTH];
        size_t copy_length = line_token.length;
        if (copy_length >= MAX_LOG_MESSAGE_LENGTH)
            copy_length = MAX_LOG_MESSAGE_LENGTH - 1;
        
        strncpy(temp_buffer, line_token.value, copy_length);
        temp_buffer[copy_length] = '\0';
        
        item->length = CStringToTChar(temp_buffer, item->value, MAX_LOG_MESSAGE_LENGTH);
    }
}

void LogViewRender(View* view, const RectInt& rect)
{
    LogView* log_view = (LogView*)view;
    u32 count = GetCount(log_view->messages);
    if (count == 0)
        return;

    u32 y = GetBottom(rect) - 1;
    u32 top = GetTop(rect);
    for (u32 i=count; i>0 && y >= top; i--, y--)
    {
        LogMessage* message = (LogMessage*)GetAt(log_view->messages, i - 1);
        WriteScreen(GetLeft(rect), y, message->value, message->length);
    }

    // TODO: need tstring working
}

static ViewTraits g_log_view_traits = {
    .render = LogViewRender
};

LogView* CreateLogView(Allocator* allocator)
{
    LogView* view = (LogView*)Alloc(allocator, sizeof(LogView));
    view->traits = &g_log_view_traits;
    view->messages = CreateRingBuffer(allocator, sizeof(LogMessage), MAX_LOG_MESSAGES);
    return view;
}
