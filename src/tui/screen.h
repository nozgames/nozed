//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

#include "tstring.h"

struct ScreenOutputBuffer
{
    char* buffer;
    size_t size;
};

void ClearScreen(TChar c);
i32 GetScreenWidth();
i32 GetScreenHeight();
Vec2Int GetWritePosition();
void WriteScreen(TChar c);
void WriteScreen(TChar* str, u32 str_len);
void WriteScreen(const char* str, TColor fg = TCOLOR_NONE);
void WriteScreen(i32 x, i32 y, const char* str, TColor fg = TCOLOR_NONE);
void WriteScreen(i32 x, i32 y, TChar c);
void WriteScreen(i32 x, i32 y, TChar* str, u32 str_len);
void DrawVerticalLine(i32 x, i32 y, i32 height, TChar c);
void DrawHorizontalLine(i32 x, i32 y, i32 width, TChar c);
void WriteColor(const RectInt& rect, TColor color);
void WriteBackgroundColor(const RectInt& rect, TColor color);
void MoveCursor(int x, int y);
void PushClipRect(const RectInt& rect, bool wrap=false);
void PopClipRect();
