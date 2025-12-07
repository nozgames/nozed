//
//  MeshZ - Copyright(c) 2025 NoZ Games, LLC
//

#include <editor.h>
#include "nozed_assets.h"

constexpr int MAX_NOTIFICATIONS = 8;
constexpr float NOTIFICATION_DURATION = 3.0f;
constexpr float NOTIFICATION_SPACING = 8.0f;
constexpr float NOTIFICATION_PADDING = 8.0f;

struct Notification
{
    char text[1024];
    float elapsed;
    NotificationType type;
};

struct NotificationSystem {
    RingBuffer* buffer;
};

static NotificationSystem g_notifications = {};

void AddNotification(NotificationType type, const char* format, ...) {
    assert(format);
    assert(g_notifications.buffer);

    if (g_notifications.buffer->count == MAX_NOTIFICATIONS)
        PopFront(g_notifications.buffer);

    Notification* n = (Notification*)PushBack(g_notifications.buffer);

    va_list args;
    va_start(args, format);
    Format(n->text, sizeof(n->text), format, args);
    va_end(args);

    n->elapsed = 0.0f;
    n->type = type;
}

void UpdateNotifications() {
    if (!IsWindowFocused())
        return;

    if (g_notifications.buffer->count <= 0)
        return;

    BeginCanvas();
    BeginContainer({.align=ALIGN_BOTTOM_RIGHT, .margin=EdgeInsetsBottomRight(STYLE_WORKSPACE_PADDING)});
    BeginColumn({.spacing=NOTIFICATION_SPACING});

    for (int i=0, c=g_notifications.buffer->count; i<c; i++) {
        Notification* n = (Notification*)GetAt(g_notifications.buffer, i);
        n->elapsed += GetFrameTime();
        if (n->elapsed > NOTIFICATION_DURATION) {
           PopFront(g_notifications.buffer);
           i--;
           c--;
           continue;
        }

        BeginContainer({
            .width=300,
            .height=40,
            .padding=EdgeInsetsAll(NOTIFICATION_PADDING),
            .color=STYLE_BACKGROUND_COLOR_LIGHT});
        Label(n->text, {
            .font=FONT_SEGUISB,
            .font_size=STYLE_TEXT_FONT_SIZE,
            .color=n->type == NOTIFICATION_TYPE_ERROR
                ? STYLE_ERROR_COLOR
                : STYLE_TEXT_COLOR,
            .align=ALIGN_CENTER_LEFT});
        EndContainer();
    }

    EndColumn();
    EndContainer();
    EndCanvas();
}

void InitNotifications() {
    g_notifications.buffer = CreateRingBuffer(ALLOCATOR_DEFAULT, sizeof(Notification), MAX_NOTIFICATIONS);
}
