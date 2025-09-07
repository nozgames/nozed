//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

namespace noz::msdf
{
    int sign(double n);
    int nonZeroSign(double n);
    double shoeLace(const Vec2Double& a, const Vec2Double& b);
    int solveQuadratic(double& x0, double& x1, double a, double b, double c);
    int solveCubicNormed(double& x0, double& x1, double& x2, double a, double b, double c);
    int solveCubic(double& x0, double& x1, double& x2, double a, double b, double c, double d);
    Vec2Double orthoNormalize(const Vec2Double& v, bool polarity = true);
    double cross(const Vec2Double& lhs, const Vec2Double& rhs);
}
