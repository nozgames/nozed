//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

void EnableShortcuts(const Shortcut* shortcuts, InputSet* input_set) {
    if (!input_set)
        input_set = g_view.input;

    for (const Shortcut* s = shortcuts; s->button != INPUT_CODE_NONE; s++)
        EnableButton(input_set, s->button);
}

void CheckShortcuts(const Shortcut* shortcuts, InputSet* input_set) {
    if (!input_set)
        input_set = g_view.input;

    // Modifiers
    bool alt = IsButtonDown(g_view.input, KEY_LEFT_ALT) || IsButtonDown(g_view.input, KEY_RIGHT_ALT);
    bool ctrl = IsButtonDown(g_view.input, KEY_LEFT_CTRL) || IsButtonDown(g_view.input, KEY_RIGHT_CTRL);
    bool shift = IsButtonDown(g_view.input, KEY_LEFT_SHIFT) || IsButtonDown(g_view.input, KEY_RIGHT_SHIFT);

    for (const Shortcut* s = shortcuts; s->button != INPUT_CODE_NONE; s++) {
        if (alt == s->alt && ctrl == s->ctrl && shift == s->shift) {
            if (WasButtonPressed(input_set, s->button)) {
                if (s->action)
                    s->action();

                return;
            }
        }
    }
}
