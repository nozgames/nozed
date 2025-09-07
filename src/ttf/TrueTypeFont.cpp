//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

using namespace std;

#include <filesystem>
#include <string>
#include "TrueTypeFont.h"
#include "TrueTypeFontReader.h"

namespace noz::ttf
{
    TrueTypeFont* TrueTypeFont::load(const string& path, int requestedSize, const string& filter)
    {
        Stream* stream = LoadStream(nullptr, filesystem::path(path.c_str()));
        if (!stream)
            return nullptr;
        
        TrueTypeFont* font = load(stream, requestedSize, filter);
        Free(stream);
        return font;
    }

    TrueTypeFont* TrueTypeFont::load(Stream* stream, int requestedSize, const string& filter)
    {
        TrueTypeFontReader reader(stream, requestedSize, filter);
        return reader.read();
    }
}
