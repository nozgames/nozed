//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "editor.h"
#include "asset_editor/asset_editor.h"

struct CommandDef
{
    const char* short_name;
    const char* name;
    void (*handler)(Tokenizer&);
};

// @quit
static void HandleQuit(Tokenizer& tk)
{
    if (g_editor.view_stack_count > 0)
        PopView();
    else
        g_editor.is_running = false;
}

// @save
static void HandleSave(Tokenizer& tk)
{
    SaveEditableAssets();
}

// @edit
static void HandleEdit(Tokenizer& tk)
{
    if (IsWindowCreated())
        FocusWindow();
    else
        InitAssetEditor();
}

static CommandDef g_commands[] = {
    { "q", "quit", HandleQuit },
    { "s", "save", HandleSave },
    { "e", "edit", HandleEdit },
    { nullptr, nullptr, nullptr }
};

void HandleCommand(const std::string& str)
{
    Tokenizer tokenizer;
    Token token;
    Init(tokenizer, str.c_str());

    if (!ExpectIdentifier(tokenizer, &token))
    {
        LogError("Unknown command");
        return;
    }

    std::string command = ToString(token);

    for (CommandDef* cmd = g_commands; cmd->name; cmd++)
    {
        if (Equals(command.c_str(), cmd->name, true) || Equals(command.c_str(), cmd->short_name))
        {
            cmd->handler(tokenizer);
            return;
        }
    }

    LogError("Unknown command: %s", command.c_str());
}
