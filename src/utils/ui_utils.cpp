//
//  NozEd - Copyright(c) 2025 NoZ Games, LLC
//

#include "nozed_assets.h"
enum ConfirmType {
    CONFIRM_TYPE_NONE,
    CONFIRM_TYPE_OK,
    CONFIRM_TYPE_YES_NO
};

struct ConfirmDialog {
    ConfirmType type = CONFIRM_TYPE_NONE;
    char message[64];
    std::function<void()> callback;
};

static ConfirmDialog g_confirm = {};

static void HandleYes(const TapDetails&) {
    g_confirm.type = CONFIRM_TYPE_NONE;
    if (g_confirm.callback)
        g_confirm.callback();
}

static void HandleNo(const TapDetails&) {
    g_confirm.type = CONFIRM_TYPE_NONE;
}

void UpdateConfirmDialog() {
    if (g_confirm.type == CONFIRM_TYPE_NONE) return;

     Canvas([] {
        GestureBlocker([]{});
        Align({.alignment = ALIGNMENT_CENTER_CENTER}, [] {
            Container({.width=400, .height=100, .color=COLOR_UI_BACKGROUND}, [] {
                Column([] {
                    Expanded();
                    Align({.alignment=ALIGNMENT_CENTER_CENTER}, [] {
                        Label(g_confirm.message, {.font = FONT_SEGUISB, .font_size=18, .color=COLOR_WHITE});
                    });
                    Expanded();
                    Align({.alignment=ALIGNMENT_CENTER_CENTER}, [] {
                        Row({.spacing=20}, [] {
                            GestureDetector({.on_tap=HandleYes}, [] {
                                Container({.width=100, .height=24}, [] {
                                    Rectangle({.color_func=GetButtonHoverColor});
                                    Label("YES", {.font = FONT_SEGUISB, .font_size=18, .color=COLOR_UI_BUTTON_TEXT, .align = ALIGNMENT_CENTER_CENTER});
                                });
                            });
                            GestureDetector({.on_tap=HandleNo}, [] {
                                Container({.width=100, .height=24}, [] {
                                    Rectangle({.color_func=GetButtonHoverColor});
                                    Label("NO", {.font = FONT_SEGUISB, .font_size=18, .color=COLOR_UI_BUTTON_TEXT, .align = ALIGNMENT_CENTER_CENTER});
                                });
                            });
                        });
                    });
                    Expanded();
                });
            });
        });
    });
}

void ShowConfirmDialog(const char* message, const std::function<void()>& callback) {
    g_confirm.type = CONFIRM_TYPE_YES_NO;
    Copy(g_confirm.message, sizeof(g_confirm.message) - 1, message);
    g_confirm.callback = callback;
}

Color GetButtonHoverColor(ElementState state, float, void*) {
    return state == ELEMENT_STATE_HOVERED ? COLOR_UI_BUTTON_HOVER : COLOR_UI_BUTTON;
}
