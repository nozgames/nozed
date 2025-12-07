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

static void HandleYes() {
    g_confirm.type = CONFIRM_TYPE_NONE;
    if (g_confirm.callback)
        g_confirm.callback();
}

static void HandleNo() {
    g_confirm.type = CONFIRM_TYPE_NONE;
}

void UpdateConfirmDialog() {
    if (g_confirm.type == CONFIRM_TYPE_NONE) return;

    BeginCanvas();
    BeginCenter();
    BeginContainer({.width=400, .height=100, .color=COLOR_UI_BACKGROUND});
    BeginColumn();
    {
        Expanded();

        // message
        BeginCenter();
        Label(g_confirm.message, {.font = FONT_SEGUISB, .font_size=18, .color=COLOR_WHITE});
        EndCenter();

        Expanded();

        // buttons
        BeginCenter();
        BeginRow({.spacing=20});

        BeginContainer({.width=100, .height=24});
        if (WasPressed()) HandleYes();
        Rectangle({.color = IsHovered() ? COLOR_UI_BUTTON_HOVER : COLOR_UI_BUTTON});
        Label("YES", {.font = FONT_SEGUISB, .font_size=18, .color=COLOR_UI_BUTTON_TEXT, .align = ALIGN_CENTER});
        EndContainer();

        BeginContainer({.width=100, .height=24});
        if (WasPressed()) HandleNo();
        Rectangle({.color = IsHovered() ? COLOR_UI_BUTTON_HOVER : COLOR_UI_BUTTON});
        Label("NO", {.font = FONT_SEGUISB, .font_size=18, .color=COLOR_UI_BUTTON_TEXT, .align = ALIGN_CENTER});
        EndContainer();

        EndRow();
        EndCenter();

        Expanded();
    }
    EndColumn();
    EndContainer();
    EndCenter();
    EndCanvas();
}

void ShowConfirmDialog(const char* message, const std::function<void()>& callback) {
    g_confirm.type = CONFIRM_TYPE_YES_NO;
    Copy(g_confirm.message, sizeof(g_confirm.message) - 1, message);
    g_confirm.callback = callback;
}
