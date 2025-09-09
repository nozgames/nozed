//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "text_input.h"

#include "screen.h"
#include "terminal.h"

struct TextInputImpl : TextInputBox
{
    std::string buffer;
    i32 cursor_pos;
    int x;
    int y;
    int width;
    bool active;
};

TextInputBox* CreateTextInput(int x, int y, int width)
{
    auto ti = new TextInputImpl;
    ti->x = x;
    ti->y = y;
    ti->width = width;
    ti->active = false;
    return ti;
}

void Destroy(TextInputBox* input)
{
    if (input)
    {
        TextInputImpl* impl = static_cast<TextInputImpl*>(input);
        delete impl;
    }
}

void Render(TextInputBox* input)
{
    assert(input);
    TextInputImpl* impl = static_cast<TextInputImpl*>(input);

    Vec2Int pos = GetWritePosition();

    WriteScreen(impl->buffer.c_str());

    if (impl->active)
    {
        WriteBackgroundColor( {pos.x + impl->cursor_pos, pos.y, 1, 1 }, TCOLOR_BACKGROUND_WHITE);
        WriteColor({pos.x + impl->cursor_pos, pos.y, 1, 1 }, TCOLOR_BLACK);
    }
}

void SetActive(TextInputBox* input, bool active)
{
    assert(input);
    auto impl = static_cast<TextInputImpl*>(input);
    impl->active = active;
}

bool HandleKey(TextInputBox* input, int key)
{
    assert(input);
    auto* impl = static_cast<TextInputImpl*>(input);

    if (!impl->active)
        return false;

    switch (key)
    {
    case 127: // Delete/Backspace
    case 8:
        if (impl->cursor_pos > 0)
        {
            impl->buffer.erase(impl->cursor_pos - 1, 1);
            impl->cursor_pos--;
        }
        return true;

    case KEY_LEFT:
        if (impl->cursor_pos > 0)
        {
            impl->cursor_pos--;
        }
        return true;

    case KEY_RIGHT:
        if (impl->cursor_pos < impl->buffer.length())
        {
            impl->cursor_pos++;
        }
        return true;

    case KEY_HOME:
        impl->cursor_pos = 0;
        return true;

    case KEY_END:
        impl->cursor_pos = impl->buffer.length();
        return true;

    default:
        // Handle regular characters
        if (key >= 32 && key <= 126)
        { // Printable ASCII
            // Ensure cursor_pos is valid
            if (impl->cursor_pos > impl->buffer.length())
                impl->cursor_pos = impl->buffer.length();

            impl->buffer.insert(impl->cursor_pos, 1, static_cast<char>(key));
            impl->cursor_pos++;
            return true;
        }
        break;
    }

    return false;
}

void Clear(TextInputBox* input)
{
    assert(input);
    TextInputImpl* impl = static_cast<TextInputImpl*>(input);
    impl->buffer.clear();
    impl->cursor_pos = 0;
}

const std::string& GetText(TextInputBox* input)
{
    assert(input);
    TextInputImpl* impl = static_cast<TextInputImpl*>(input);
    return impl->buffer;
}

void SetText(TextInputBox* input, const std::string& text)
{
    assert(input);
    TextInputImpl* impl = static_cast<TextInputImpl*>(input);
    impl->buffer = text;
    impl->cursor_pos = impl->buffer.length();
}

size_t GetCursorPos(TextInputBox* input)
{
    assert(input);
    TextInputImpl* impl = static_cast<TextInputImpl*>(input);
    return impl->cursor_pos;
}

bool IsActive(TextInputBox* input)
{
    assert(input);
    TextInputImpl* impl = static_cast<TextInputImpl*>(input);
    return impl->active;
}