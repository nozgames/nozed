/*

    NoZ Game Engine

    Copyright(c) 2025 NoZ Games, LLC

*/

#pragma once

#include "Edge.h"

namespace noz::msdf
{
    struct Contour
    {
        ~Contour();

        void bounds(double& l, double& b, double& r, double& t);
        int winding();

        std::vector<Edge*> edges;
    };
}
