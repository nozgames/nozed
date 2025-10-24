//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "nozed_assets.h"

#include <editor.h>

struct CommandHandler
{
    const Name* short_name;
    const Name* name;
    void (*handler)(const Command&);
};

static CommandHandler* g_commands = nullptr;

// @save
static void HandleSave(const Command& command)
{
    (void)command;
    SaveEditorAssets();
}

// @new
static void HandleNew(const Command& command)
{
    if (command.arg_count < 1)
    {
        LogError("missing asset type (mesh, etc)");
        return;
    }

    const Name* type = GetName(command.args[0]);

    if (command.arg_count < 2)
    {
        LogError("missing asset name");
        return;
    }

    const Name* asset_name = GetName(command.args[1]);

    AssetData* ea = nullptr;
    if (type == NAME_MESH || type == NAME_M)
        ea = NewEditorMesh(asset_name->value);
    else if (type == NAME_SKELETON || type == NAME_S)
        ea = NewEditorSkeleton(asset_name->value);
    else if (type == NAME_ANIMATION || type == NAME_A)
        ea = NewEditorAnimation(asset_name->value);

    if (ea == nullptr)
        return;

    ea->position = GetCenter(GetBounds(g_view.camera));
    ea->meta_modified = true;

    if (ea->vtable.post_load)
        ea->vtable.post_load(ea);
}

// @rename
static void HandleRename(const Command& command)
{
    if (command.arg_count < 1)
    {
        LogError("missing name");
        return;
    }

    HandleRename(GetName(command.args[0]));
}

void HandleCommand(const Command& command)
{
    for (CommandHandler* cmd = g_commands; cmd->name; cmd++)
    {
        if (command.name == cmd->name || command.name == cmd->short_name)
        {
            cmd->handler(command);
            return;
        }
    }

    LogError("Unknown command: %s", command.name->value);
}

bool ParseCommand(const char* str, Command& command)
{
    Tokenizer tk;
    Init(tk, str);

    if (!ExpectIdentifier(tk))
        return NAME_NONE;

    command.arg_count = 0;
    command.name = GetName(tk);

    Token token = {};
    while (ExpectToken(tk, &token))
        GetString(token, command.args[command.arg_count++], MAX_COMMAND_ARG_SIZE);

    if (command.arg_count <= 0 && tk.input[tk.position-1] != ' ')
        return false;

    return true;
}

void InitCommands()
{
    static CommandHandler commands[] = {
        { NAME_S, NAME_SAVE, HandleSave },
        { NAME_N, NAME_NEW, HandleNew },
        { NAME_R, NAME_RENAME, HandleRename },
        { nullptr, nullptr, nullptr }
    };

    g_commands = commands;
}