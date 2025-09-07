//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

#include "tstring.h"

// Special key codes (using values that don't conflict with ASCII)
#define KEY_LEFT    260
#define KEY_RIGHT   261
#define KEY_UP      262
#define KEY_DOWN    263
#define KEY_HOME    264
#define KEY_END     265
#define KEY_PPAGE   266
#define KEY_NPAGE   267

using TerminalRenderCallback = std::function<void(int width, int height)>;
using TerminalResizeCallback = std::function<void(int new_width, int new_height)>;

void InitTerminal();
void ShutdownTerminal();
void RequestRender();
void RenderTerminal();
void UpdateTerminal();

void SetRenderCallback(TerminalRenderCallback callback);
void SetResizeCallback(TerminalResizeCallback callback);
void ClearRect(const Rect& rect);
void AddChar(char ch);
void AddChar(char ch, int count);
void AddString(const char* str);
void AddString(const TString& tstr, int cursor_pos, int truncate=1000000);
void BeginInverse();
void EndInverse();
void SetColor(int pair);
void UnsetColor(int pair);
void SetColor256(int fg, int bg = -1);                    // 256-color mode (0-255)
void SetColorRGB(int r, int g, int b, int bg_r = -1, int bg_g = -1, int bg_b = -1);  // RGB mode
void BeginColor(int r, int g, int b);                     // Begin RGB color
void EndColor();                                          // Reset to default color
void SetBold(bool enabled);
void SetUnderline(bool enabled);
void SetItalic(bool enabled);
void SetScrollRegion(int top, int bottom);               // Set scrolling region (1-based lines)
void ResetScrollRegion();                                // Reset to full screen scrolling
void SetCursorVisible(bool visible);
bool HasColorSupport();
int GetCursorX();
int GetTerminalKey();



