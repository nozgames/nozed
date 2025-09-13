//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include <view.h>
#include "editor.h"

struct CommandDef
{
    const char* short_name;
    const char* name;
    void (*handler)(Tokenizer&);
};

// @quit
static void HandleQuit(Tokenizer& tk)
{
    (void)tk;
    g_editor.is_running = false;
}

// @save
static void HandleSave(Tokenizer& tk)
{
    (void)tk;
    SaveEditorAssets();
}

// @edit
static void HandleEdit(Tokenizer& tk)
{
    if (IsWindowCreated())
        FocusWindow();
    else
        InitView();

    if (!ExpectIdentifier(tk))
        return;

    const Name* name = GetName(tk);
    if (name == NAME_NONE)
        return;

    int asset_index = FindEditorAssetByName(name);
    if (asset_index == -1)
    {
        LogError("unknown asset: %s", name->value);
        return;
    }

    FocusAsset(asset_index);
}

// @new
static void HandleNew(Tokenizer& tk)
{
    if (!ExpectIdentifier(tk))
    {
        LogError("missing asset type (mesh, etc)");
        return;
    }

    std::string type_name = GetString(tk);
    if (!ExpectLine(tk))
    {
        LogError("missing asset name");
        return;
    }

    const Name* name = GetName(tk);
    if (name == NAME_NONE)
        return;

    if (type_name == "mesh")
        NewEditorMesh(name->value);
    else if (type_name == "skeleton")
        NewEditorSkeleton(name->value);
}

// @rename
static void HandleRename(Tokenizer& tk)
{
    if (!ExpectIdentifier(tk))
    {
        LogError("missing name");
        return;
    }

    // todo: route to the asset editor
}

static CommandDef g_commands[] = {
    { "q", "quit", HandleQuit },
    { "s", "save", HandleSave },
    { "e", "edit", HandleEdit },
    { "n", "new", HandleNew },
    { "r", "rename", HandleRename },
    { nullptr, nullptr, nullptr }
};

void HandleCommand(const std::string& str)
{
    Tokenizer tk;
    Init(tk, str.c_str());

    if (!ExpectIdentifier(tk))
    {
        LogError("Unknown command");
        return;
    }

    std::string command = GetString(tk);

    for (CommandDef* cmd = g_commands; cmd->name; cmd++)
    {
        if (Equals(command.c_str(), cmd->name, true) || Equals(command.c_str(), cmd->short_name))
        {
            cmd->handler(tk);
            return;
        }
    }

    LogError("Unknown command: %s", command.c_str());
}
