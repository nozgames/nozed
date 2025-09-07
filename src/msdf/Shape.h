/*
  NoZ Game Engine

  Copyright(c) 2025 NoZ Games, LLC

*/

#include "Contour.h"
#include "../ttf/TrueTypeFont.h"

namespace noz::msdf
{
    struct Shape
    {
        bool validate();
        void normalize();
        void bounds(double& l, double& b, double& r, double& t);

        ~Shape();

        static Shape* fromGlyph(const ttf::TrueTypeFont::Glyph* glyph, bool invertYAxis);

        std::vector<Contour*> contours;
        bool inverseYAxis = false;
    };
}
