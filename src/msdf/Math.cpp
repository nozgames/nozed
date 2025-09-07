//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

namespace noz::msdf
{
    int sign(double n)
    {
        return (0.0 < n ? 1 : 0) - (n < 0.0 ? 1 : 0);
    }

    int nonZeroSign(double n)
    {
        return 2 * (n > 0.0 ? 1 : 0) - 1;
    }

    double shoeLace(const Vec2Double& a, const Vec2Double& b)
    {
        return (b.x - a.x) * (a.y + b.y);
    }

    int solveQuadratic(double& x0, double& x1, double a, double b, double c)
    {
        if (abs(a) < 1e-14)
        {
            if (abs(b) < 1e-14)
            {
                if (c == 0)
                    return -1;
                return 0;
            }
            x0 = -c / b;
            return 1;
        }
        double dscr = b * b - 4 * a * c;
        if (dscr > 0)
        {
            dscr = sqrt(dscr);
            x0 = (-b + dscr) / (2 * a);
            x1 = (-b - dscr) / (2 * a);
            return 2;
        }
        else if (dscr == 0)
        {
            x0 = -b / (2 * a);
            return 1;
        }
        else
            return 0;
    }

    int solveCubicNormed(double& x0, double& x1, double& x2, double a, double b, double c)
    {
        double a2 = a * a;
        double q = (a2 - 3 * b) / 9;
        double r = (a * (2 * a2 - 9 * b) + 27 * c) / 54;
        double r2 = r * r;
        double q3 = q * q * q;
        double A, B;
        if (r2 < q3)
        {
            double t = r / sqrt(q3);
            if (t < -1)
                t = -1;
            if (t > 1)
                t = 1;
            t = acos(t);
            a /= 3;
            q = -2 * sqrt(q);
            x0 = q * cos(t / 3) - a;
            x1 = q * cos((t + 2 * noz::PI) / 3) - a;
            x2 = q * cos((t - 2 * noz::PI) / 3) - a;
            return 3;
        }
        else
        {
            A = -pow(abs(r) + sqrt(r2 - q3), 1 / 3.0);
            if (r < 0)
                A = -A;
            B = A == 0 ? 0 : q / A;
            a /= 3;
            x0 = (A + B) - a;
            x1 = -0.5f * (A + B) - a;
            x2 = 0.5f * sqrt(3.0f) * (A - B);
            if (abs(x2) < 1e-14)
                return 2;

            return 1;
        }
    }

    int solveCubic(double& x0, double& x1, double& x2, double a, double b, double c, double d)
    {
        if (abs(a) < 1e-14)
            return solveQuadratic(x0, x1, b, c, d);

        return solveCubicNormed(x0, x1, x2, b / a, c / a, d / a);
    }

    Vec2Double orthoNormalize(const Vec2Double& v, bool polarity)
    {
        double len = Length(v);
        return (polarity ? 1.0 : -1.0) * Vec2Double(-v.y / len, v.x / len);
    }

    double cross(const Vec2Double& lhs, const Vec2Double& rhs)
    {
        return (lhs.x * rhs.y) - (lhs.y * rhs.x);
    }
}
