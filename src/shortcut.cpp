//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include <view.h>
#include "noz/input_code.h"

void EnableShortcuts(const Shortcut* shortcuts)
{
    for (const Shortcut* s = shortcuts; s->button != INPUT_CODE_NONE; s++)
        EnableButton(g_view.input, s->button);
}

void CheckShortcuts(const Shortcut* shortcuts)
{
    // Modifiers
    bool alt = IsButtonDown(g_view.input, KEY_LEFT_ALT) || IsButtonDown(g_view.input, KEY_RIGHT_ALT);
    bool ctrl = IsButtonDown(g_view.input, KEY_LEFT_CTRL) || IsButtonDown(g_view.input, KEY_RIGHT_CTRL);
    bool shift = IsButtonDown(g_view.input, KEY_LEFT_SHIFT) || IsButtonDown(g_view.input, KEY_RIGHT_SHIFT);

    for (const Shortcut* s = shortcuts; s->button != INPUT_CODE_NONE; s++)
    {
        if (alt == s->alt && ctrl == s->ctrl && shift == s->shift)
        {
            if (WasButtonPressed(g_view.input, s->button))
            {
                if (s->action)
                    s->action();

                return;
            }
        }
    }
}
