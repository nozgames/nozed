//
//  MeshZ - Copyright(c) 2025 NoZ Games, LLC
//

#include <editor.h>
#include "editor_assets.h"

constexpr int MAX_NOTIFICATIONS = 8;
constexpr float NOTIFICATION_DURATION = 3.0f;

struct Notification
{
    char text[1024];
    float elapsed;
};

struct NotificationSystem
{
    RingBuffer* buffer;
};

static NotificationSystem g_notifications = {};

void AddNotification(const char* format, ...)
{
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
}

void UpdateNotifications() {
    Canvas([] {
        Align({.alignment = ALIGNMENT_BOTTOM_RIGHT, .margin = EdgeInsetsBottomRight(10)}, [] {
           Column({.spacing = 10}, [] {
               for (int i=0, c=g_notifications.buffer->count; i<c; i++) {
                   Notification* n = (Notification*)GetAt(g_notifications.buffer, i);
                   n->elapsed += GetFrameTime();
                   if (n->elapsed > NOTIFICATION_DURATION) {
                       PopFront(g_notifications.buffer);
                       i--;
                       c--;
                       continue;
                   }

                   Container({.width=300, .height=40, .padding=EdgeInsetsAll(10), .color=COLOR_UI_BACKGROUND}, [n] {
                        Label(n->text, {.font = FONT_SEGUISB, .font_size=18, .color=COLOR_WHITE});
                   });
               }
           });
        });
    });
}

void InitNotifications() {
    g_notifications.buffer = CreateRingBuffer(ALLOCATOR_DEFAULT, sizeof(Notification), MAX_NOTIFICATIONS);
}
