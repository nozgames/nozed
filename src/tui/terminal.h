//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

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

extern void InitTerminal();
extern void ShutdownTerminal();
extern void RenderTerminal();
extern void UpdateTerminal();

extern void SetRenderCallback(TerminalRenderCallback callback);
extern void SetResizeCallback(TerminalResizeCallback callback);
extern int GetTerminalKey();
