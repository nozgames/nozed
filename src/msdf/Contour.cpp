/*

    NoZ Game Engine

    Copyright(c) 2025 NoZ Games, LLC

*/

#include "Contour.h"
#include "Math.h"

namespace noz::msdf
{
    Contour::~Contour()
    {
        for (auto& edge : edges)
            delete edge;
    }

    void Contour::bounds(double& l, double& b, double& r, double& t)
    {
        for (auto& edge : edges)
            edge->bounds(l, b, r, t);
    }

    int Contour::winding()
    {
        if (edges.size() == 0)
            return 0;

        double total = 0;
        if (edges.size() == 1)
        {
            auto a = edges[0]->point(0);
            auto b = edges[0]->point(1 / 3.0);
            auto c = edges[0]->point(2 / 3.0);
            total += shoeLace(a, b);
            total += shoeLace(b, c);
            total += shoeLace(c, a);
        }
        else if (edges.size() == 2)
        {
            auto a = edges[0]->point(0);
            auto b = edges[0]->point(0.5);
            auto c = edges[1]->point(0);
            auto d = edges[1]->point(.5);
            total += shoeLace(a, b);
            total += shoeLace(b, c);
            total += shoeLace(c, d);
            total += shoeLace(d, a);
        }
        else
        {
            auto prev = edges.back()->point(0);
            for(auto edge : edges)
            {
                auto cur = edge->point(0);
                total += shoeLace(prev, cur);
                prev = cur;
            }
        }
        return sign(total);
    }
}
