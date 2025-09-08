//
//  MeshZ - Copyright(c) 2025 NoZ Games, LLC
//

#include "asset_editor.h"

#include <cstdarg>

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
    struct
    {
        const Name* notification_container;
        const Name* notification;
        const Name* notification_text;
    } names;
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

void UpdateNotifications()
{
    BeginCanvas(UI_REF_WIDTH, UI_REF_HEIGHT);
    SetStyleSheet(g_assets.ui.notifications);
    BeginElement(g_notifications.names.notification_container);
        for (int i=0, c=g_notifications.buffer->count; i<c; i++)
        {
            Notification* n = (Notification*)GetAt(g_notifications.buffer, i);
            n->elapsed += GetFrameTime();
            if (n->elapsed > NOTIFICATION_DURATION)
            {
                PopFront(g_notifications.buffer);
                i--;
                c--;
                continue;
            }

            BeginElement(g_notifications.names.notification);
                Label(n->text, g_notifications.names.notification_text);
            EndElement();
        }
    EndElement();
    EndCanvas();
}

void InitNotifications()
{
    g_notifications.buffer = CreateRingBuffer(ALLOCATOR_DEFAULT, sizeof(Notification), MAX_NOTIFICATIONS);
    g_notifications.names.notification = GetName("notification");
    g_notifications.names.notification_container = GetName("notification_container");
    g_notifications.names.notification_text = GetName("notification_text");
}
