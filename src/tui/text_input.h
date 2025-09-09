//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

#include <string>

struct TextInputBox
{

};

TextInputBox* CreateTextInput(int x, int y, int width);
void Destroy(TextInputBox* input);
void Render(TextInputBox* input);
void SetActive(TextInputBox* input, bool active);
bool HandleKey(TextInputBox* input, int key);
void Clear(TextInputBox* input);
const std::string& GetText(TextInputBox* input);
void SetText(TextInputBox* input, const std::string& text);
size_t GetCursorPos(TextInputBox* input);
bool IsActive(TextInputBox* input);