//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "screen.h"

#include <enet/enet.h>
#include <vector>

constexpr int OUTPUT_BUFFER_SIZE = 1024 * 1024 * 16;

struct Clip
{
    RectInt rect;
    bool wrap;
};

static TChar* g_buffer = nullptr;
static char g_output_buffer[OUTPUT_BUFFER_SIZE];
static i32 g_screen_width = 0;
static i32 g_screen_height = 0;
static Vec2Int g_cursor = {0,0};
static std::vector<Clip> g_clip;

static RectInt Clip(const RectInt& rect)
{
    assert(g_clip.size() > 0);
    auto& clip = g_clip.back().rect;
    RectInt r;
    r.x = Max(rect.x, clip.x);
    r.y = Max(rect.y, clip.y);
    r.width = Min(rect.x + rect.width, clip.x + clip.width) - r.x;
    r.height = Min(rect.y + rect.height, clip.y + clip.height) - r.y;
    return r;
}

static Vec2Int Clip(const Vec2Int& pt)
{
    assert(g_clip.size() > 0);
    auto& clip = g_clip.back().rect;
    Vec2Int r;
    r.x = Clamp(pt.x, clip.x, GetRight(clip) - 1);
    r.y = Clamp(pt.y, clip.y, GetBottom(clip) - 1);
    return r;
}

Vec2Int GetWritePosition()
{
    return g_cursor;
}

int GetScreenWidth()
{
    return g_screen_width;
}

int GetScreenHeight()
{
    return g_screen_height;
}

void MoveCursor(i32 x, i32 y)
{
    g_cursor = Clip(Vec2Int{x,y});
}

void WriteScreen(TChar c)
{
    g_buffer[g_cursor.y * g_screen_width + g_cursor.x] = c;
    MoveCursor(g_cursor.x + 1, g_cursor.y);
}

void WriteScreen(TChar* str, u32 str_len)
{
    for (u32 i=0; i<str_len; ++i, str++)
        WriteScreen(*str);
}

void WriteScreen(const char* str, TColor fg)
{
    TChar temp[4096];
    u32 len = CStringToTChar(str, temp, 4095, fg);
    WriteScreen(temp, len);
}

void WriteScreen(i32 x, i32 y, const char* str, TColor fg)
{
    MoveCursor(x,y);
    WriteScreen(str, fg);
}

void WriteScreen(i32 x, i32 y, TChar c)
{
    MoveCursor(x, y);
    WriteScreen(c);
}

void WriteScreen(i32 x, i32 y, TChar* str, u32 str_len)
{
    assert(str);
    MoveCursor(x,y);
    WriteScreen(str, str_len);
}

void PushClipRect(const RectInt& rect, bool wrap)
{
    g_clip.push_back({rect, wrap});
}

void PopClipRect()
{
    assert(g_clip.size() > 0);
    g_clip.pop_back();
}

void DrawVerticalLine(i32 x, i32 y, i32 height, TChar c)
{
    Vec2Int pos_t = Clip(Vec2Int{x,y});
    Vec2Int pos_b = Clip(Vec2Int{x,y + height});
    for (y=pos_t.y;y<pos_b.y; y++)
        WriteScreen(x, y, c);
}

void DrawHorizontalLine(i32 x, i32 y, i32 width, TChar c)
{
    Vec2Int pos_l = Clip(Vec2Int{x,y});
    Vec2Int pos_r = Clip(Vec2Int{x + width,y});
    for (x=pos_l.x;x<pos_r.x; x++)
        WriteScreen(x, y, c);
}

void WriteColor(const RectInt& rect, TColor color)
{
    auto clip = Clip(rect);
    for (int y = clip.y, yy = GetBottom(clip); y < yy; y++)
        for (int x = clip.x, xx = GetRight(clip); x < xx; x++)
            g_buffer[y * g_screen_width + x].fg_color = color;
}

void WriteBackgroundColor(const RectInt& rect, TColor color)
{
    auto clip = Clip(rect);
    for (int y = clip.y, yy = GetBottom(clip); y < yy; y++)
        for (int x = clip.x, xx = GetRight(clip); x < xx; x++)
            g_buffer[y * g_screen_width + x].bg_color = color;
}

void UpdateScreenSize(i32 width, i32 height)
{
    g_clip.clear();
    g_clip.push_back({0,0,width,height});

    auto old_buffer = g_buffer;
    auto new_buffer = (TChar*)calloc(1, width * height * sizeof(TChar));

    if (old_buffer)
    {
        i32 yy = Min(height, g_screen_height);
        i32 xx = Min(width, g_screen_width);
        for (int y = 0; y < yy; y++)
        {
            auto old_row = old_buffer + y * g_screen_width;
            auto new_row = new_buffer + y * width;
            memcpy(new_row, old_row, xx * sizeof(TChar));
        }

        free(old_buffer);
    }

    g_buffer = new_buffer;
    g_screen_width = width;
    g_screen_height = height;
}

void ClearScreen(TChar c)
{
    assert(g_clip.size() > 0);
    assert(g_buffer);

    auto& clip = g_clip.back();
    auto clip_l = GetLeft(clip.rect);
    auto clip_r = GetRight(clip.rect);
    auto clip_t = GetTop(clip.rect);
    auto clip_b = GetBottom(clip.rect);

    if (clip_l >= clip_r || clip_t >= clip_b)
        return;

    for (int y = clip_t; y < clip_b; y++)
        for (int x = clip_l; x < clip_r; x++)
            g_buffer[y * g_screen_width + x] = c;

    MoveCursor(clip_l, clip_t);
}

static char* RenderEscape(char* o)
{
    *o++ = '\033';
    *o++ = '[';
    return o;
}

static char* RenderInt(char* o, int i)
{
    auto len = snprintf(o, OUTPUT_BUFFER_SIZE - (o - g_output_buffer)-1, "%d", i);
    o += len;
    return o;
}

static char* RenderColor(char* o, const TColor& color)
{
    o = RenderEscape(o);
    o = RenderInt(o, color.code);

    if (color.code == 38 || color.code == 48)
    {
        *o++ = ';';
        *o++ = '2';
        *o++ = ';';
        o = RenderInt(o, color.r);
        *o++ = ';';
        o = RenderInt(o, color.g);
        *o++ = ';';
        o = RenderInt(o, color.b);
    }

    *o++ = 'm';
    return o;
}

static char* RenderMoveCursor(char* o, int x, int y)
{
    o = RenderEscape(o);
    o = RenderInt(o, y + 1);
    *o++ = ';';
    o = RenderInt(o, x + 1);
    *o++ = 'H';
    return o;
}

ScreenOutputBuffer RenderScreen()
{
    auto o = g_output_buffer;
    TColor fg_color = TCOLOR_NONE;
    TColor bg_color = TCOLOR_BACKGROUND_NONE;
    o = RenderColor(o, fg_color);
    o = RenderColor(o, bg_color);
    for (auto y = 0; y < g_screen_height; y++)
    {
        TChar* p = g_buffer + y * g_screen_width;
        o = RenderMoveCursor(o, 0, y);
        for (auto x = 0; x < g_screen_width; x++, p++)
        {
            if (p->fg_color != fg_color)
            {
                fg_color = p->fg_color;
                o = RenderColor(o, p->fg_color);
            }

            if (p->bg_color != bg_color)
            {
                bg_color = p->bg_color;
                o = RenderColor(o, p->bg_color);
            }

            *o++ = p->value;
        }
    }

    return { g_output_buffer, (size_t)((u8*)o - (u8*)g_output_buffer) };
}

void InitScreen(i32 width, i32 height)
{

    UpdateScreenSize(width, height);
    ClearScreen(TCHAR_NONE);
}

void ShutdownScreen()
{
    if (g_buffer)
        free(g_buffer);

    g_buffer = nullptr;
    g_screen_width = 0;
    g_screen_height = 0;
}
