//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

struct CommandInput {
    const CommandHandler* commands = nullptr;
    const char* prefix = nullptr;
    const char* placeholder = nullptr;
    bool enabled;
    bool hide_empty;
    InputSet* input;
    Command command;
    bool pop_input;
};

static CommandInput g_command_input = {};

bool IsCommandInputActive() {
    return g_command_input.enabled;
}

static void HandleCommand(const Command& command) {
    for (const CommandHandler* cmd = g_command_input.commands; cmd->name != nullptr; cmd++) {
        if (cmd->name == NAME_NONE || command.name == cmd->name || command.name == cmd->short_name) {
            cmd->handler(command);
            return;
        }
    }

    LogError("Unknown command: %s", command.name->value);
}

static bool ParseCommand(const char* str, Command& command) {
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

constexpr int   COMMAND_FONT_SIZE = 24;
constexpr float COMMAND_HEIGHT = 40.0f;
constexpr float COMMAND_WIDTH = 600.0f;
constexpr float COMMAND_PADDING = 8.0f;

void UpdateCommandInput() {
    if (!g_command_input.enabled)
        return;

    if (g_command_input.hide_empty && GetTextInput().value.length == 0)
        return;

    BeginCanvas();
    BeginAlign({.alignment={.y=1}, .margin=EdgeInsetsBottom(160)});
    BeginContainer({
        .width=COMMAND_WIDTH,
        .height=COMMAND_HEIGHT,
        .padding=EdgeInsetsLeft(COMMAND_PADDING),
        .color=COLOR_UI_BACKGROUND});
        BeginRow();
            const TextInput& i = GetTextInput();
            if (g_command_input.prefix) {
                Label(g_command_input.prefix, {
                    .font = FONT_SEGUISB,
                    .font_size = COMMAND_FONT_SIZE,
                    .color = Color24ToColor(0x777776),
                    .align = ALIGNMENT_CENTER_LEFT
                });
                SizedBox({.width = 5.0f});
            }

            bool show_cursor = true;
            if (i.value.length > 0) {
                Label(i.value, {
                    .font = FONT_SEGUISB,
                    .font_size = COMMAND_FONT_SIZE,
                    .color = COLOR_UI_TEXT,
                    .align = ALIGNMENT_CENTER_LEFT
                });
            } else if (g_command_input.placeholder) {
                show_cursor = false;
                BeginContainer({.color = COLOR_UI_TEXT});
                    Label(g_command_input.placeholder, {
                        .font = FONT_SEGUISB,
                        .font_size = COMMAND_FONT_SIZE,
                        .color = COLOR_UI_BACKGROUND,
                        .align = ALIGNMENT_CENTER_LEFT
                    });
                End();
            }

            if (show_cursor) {
                BeginAlign({.alignment=ALIGNMENT_CENTER_LEFT});
                    Container({.width=4, .height=COMMAND_FONT_SIZE, .color=COLOR_WHITE});
                End();
            }
        End(); // Row
    End(); // Container
    End(); // Align
    End(); // Canvas
}

static void HandleTextInputChange(EventId event_id, const void* event_data) {
    (void)event_id;
    ParseCommand(static_cast<const TextInput*>(event_data)->value.value, g_command_input.command);
}

static void HandleTextInputCancel(EventId event_id, const void* event_data) {
    (void) event_id;
    (void) event_data;

    const TextInput& input = GetTextInput();
    if (input.value.length > 0) {
        ClearTextInput();
        return;
    }

    EndCommandInput();
}

static void HandleTextInputCommit(EventId event_id, const void* event_data) {
    (void) event_id;
    (void) event_data;

    HandleCommand(g_command_input.command);
    EndCommandInput();
}

void BeginCommandInput(const CommandInputOptions& options) {
    g_command_input.enabled = true;
    g_command_input.commands = options.commands;
    g_command_input.prefix = options.prefix;
    g_command_input.placeholder = options.placeholder;
    g_command_input.hide_empty = options.hide_empty;

    if (!options.input) {
        PushInputSet(g_command_input.input);
        g_command_input.pop_input = true;
    } else if (options.input != GetInputSet()) {
        PushInputSet(options.input);
        g_command_input.pop_input = true;
    } else {
        g_command_input.pop_input = false;
    }

    Listen(EVENT_TEXTINPUT_CHANGE, HandleTextInputChange);
    Listen(EVENT_TEXTINPUT_CANCEL, HandleTextInputCancel);
    Listen(EVENT_TEXTINPUT_COMMIT, HandleTextInputCommit);
    BeginTextInput();
}

void EndCommandInput() {
    Unlisten(EVENT_TEXTINPUT_CHANGE, HandleTextInputChange);
    Unlisten(EVENT_TEXTINPUT_CANCEL, HandleTextInputCancel);
    Unlisten(EVENT_TEXTINPUT_COMMIT, HandleTextInputCommit);

    if (g_command_input.pop_input)
        PopInputSet();

    EndTextInput();
    g_command_input.enabled = false;
    g_command_input.commands = nullptr;
    g_command_input.hide_empty = false;
    g_command_input.prefix = nullptr;
    g_command_input.placeholder = nullptr;
}

void InitCommandInput() {
    g_command_input = {};
    g_command_input.input = CreateInputSet(ALLOCATOR_DEFAULT);
    EnableButton(g_command_input.input, KEY_ESCAPE);
    EnableButton(g_command_input.input, KEY_ENTER);
}

void ShutdownCommandInput() {
    Free(g_command_input.input);
    g_command_input = {};
}
