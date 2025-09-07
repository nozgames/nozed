#pragma once

#include "../ttf/TrueTypeFont.h"

namespace noz::msdf
{
void renderGlyph(
    const ttf::TrueTypeFont::Glyph* glyph,
    std::vector<uint8_t>& output,
    int outputStride,
    const Vec2Int& outputPosition,
    const Vec2Int& outputSize,
    double range,
    const Vec2Double& scale,
    const Vec2Double& translate);
}
