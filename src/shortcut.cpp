//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

void EnableShortcuts(const Shortcut* shortcuts, InputSet* input_set) {
    if (!input_set)
        input_set = g_view.input;

    for (const Shortcut* s = shortcuts; s->button != INPUT_CODE_NONE; s++) {
        if (s->ctrl) {
            EnableButton(input_set, KEY_LEFT_CTRL);
            EnableButton(input_set, KEY_RIGHT_CTRL);
        }

        if (s->alt) {
            EnableButton(input_set, KEY_LEFT_ALT);
            EnableButton(input_set, KEY_RIGHT_ALT);
        }

        if (s->shift) {
            EnableButton(input_set, KEY_LEFT_SHIFT);
            EnableButton(input_set, KEY_RIGHT_SHIFT);
        }

        EnableButton(input_set, s->button);
    }
}

void CheckShortcuts(const Shortcut* shortcuts, InputSet* input_set) {
    if (!input_set)
        input_set = g_view.input;

    if (!IsActive(input_set))
        return;

    // Modifiers
    bool alt = IsButtonDown(input_set, KEY_LEFT_ALT) || IsButtonDown(g_view.input, KEY_RIGHT_ALT);
    bool ctrl = IsButtonDown(input_set, KEY_LEFT_CTRL) || IsButtonDown(g_view.input, KEY_RIGHT_CTRL);
    bool shift = IsButtonDown(input_set, KEY_LEFT_SHIFT) || IsButtonDown(g_view.input, KEY_RIGHT_SHIFT);

    LogInfo("%d %d %d", (int)alt, (int)ctrl, (int)shift);

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
