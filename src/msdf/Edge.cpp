//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "Edge.h"
#include "Math.h"

namespace noz::msdf
{
    Edge::Edge(EdgeColor color)
    {
        this->color = color;
    }

    void Edge::bounds(const Vec2Double& p, double& l, double& b, double& r, double& t)
    {
        if (p.x < l)
            l = p.x;
        if (p.y < b)
            b = p.y;
        if (p.x > r)
            r = p.x;
        if (p.y > t)
            t = p.y;
    }

    LinearEdge::LinearEdge(const Vec2Double& p0, const Vec2Double& p1) : LinearEdge(p0, p1, EdgeColor::White)
    {
    }

    LinearEdge::LinearEdge(const Vec2Double& p0, const Vec2Double& p1, EdgeColor color) : Edge(color)
    {
        this->p0 = p0;
        this->p1 = p1;
    }

    Vec2Double LinearEdge::point(double mix) const
    {
        return Mix(p0, p1, mix);
    }

    void LinearEdge::splitInThirds(std::vector<Edge*>& edges) const
    {
        edges.push_back(new LinearEdge(p0, point(1 / 3.0), color));
        edges.push_back(new LinearEdge(point(1 / 3.0), point(2 / 3.0), color));
        edges.push_back(new LinearEdge(point(2 / 3.0), p1, color));
    }

    void LinearEdge::bounds(double& l, double& b, double& r, double& t) const
    {
        Edge::bounds(p0, l, b, r, t);
        Edge::bounds(p1, l, b, r, t);
    }

    SignedDistance LinearEdge::distance(const Vec2Double& origin, double& param) const
    {
        Vec2Double aq = origin - p0;
        Vec2Double ab = p1 - p0;
        param = Dot(aq, ab) / Dot(ab, ab);
        Vec2Double eq = (param > 0.5 ? p1 : p0) - origin;
        double endpointDistance = Length(eq);
        if (param > 0 && param < 1)
        {
            double orthoDistance = Dot(orthoNormalize(ab, false), aq);
            if (abs(orthoDistance) < endpointDistance)
                return SignedDistance(orthoDistance, 0);
        }
        return SignedDistance(
            nonZeroSign(cross(aq, ab)) * endpointDistance,
            abs(Dot(Normalize(ab), Normalize(eq)))
        );
    }

    QuadraticEdge::QuadraticEdge(const Vec2Double& p0, const Vec2Double& p1, const Vec2Double& p2)
        : QuadraticEdge(p0, p1, p2, EdgeColor::White)
    {
    }

    QuadraticEdge::QuadraticEdge(const Vec2Double& p0, const Vec2Double& p1, const Vec2Double& p2, EdgeColor color)
        : Edge(color)
    {
        this->p0 = p0;
        this->p1 = p1;
        this->p2 = p2;

        if (p1 == p0 || p1 == p2)
            this->p1 = 0.5 * (p0 + p2);
    }

    Vec2Double QuadraticEdge::point(double mix) const
    {
        return Mix(Mix(p0, p1, mix), Mix(p1, p2, mix), mix);
    }

    void QuadraticEdge::splitInThirds(std::vector<Edge*>& result) const
    {
        result.push_back(new QuadraticEdge(p0, Mix(p0, p1, 1 / 3.0), point(1 / 3.0), color));
        result.push_back(new QuadraticEdge(point(1 / 3.0), Mix(Mix(p0, p1, 5 / 9.0), Mix(p1, p2, 4 / 9.0), .5), point(2 / 3.0), color));
        result.push_back(new QuadraticEdge(point(2 / 3.0), Mix(p1, p2, 2 / 3.0), p2, color));
    }

    void QuadraticEdge::bounds(double& l, double& b, double& r, double& t) const
    {
        Edge::bounds(p0, l, b, r, t);
        Edge::bounds(p2, l, b, r, t);

        Vec2Double bot = (p1 - p0) - (p2 - p1);
        if (bot.x != 0.0)
        {
            double param = (p1.x - p0.x) / bot.x;
            if (param > 0 && param < 1)
                Edge::bounds(point(param), l, b, r, t);
        }
        if (bot.y != 0.0)
        {
            double param = (p1.y - p0.y) / bot.y;
            if (param > 0 && param < 1)
                Edge::bounds(point(param), l, b, r, t);
        }
    }

    double QuadraticEdge::applySolution(
        double t,
        double oldSolution,
        const Vec2Double& ab,
        const Vec2Double& br,
        const Vec2Double& origin,
        double& minDistance) const
    {
        if (t > 0 && t < 1)
        {
            Vec2Double endpoint = p0 + 2 * t * ab + t * t * br;
            double distance = nonZeroSign(cross(p2 - p0, endpoint - origin)) * Length(endpoint - origin);
            if (abs(distance) <= abs(minDistance))
            {
                minDistance = distance;
                return t;
            }
        }

        return oldSolution;
    }

    SignedDistance QuadraticEdge::distance(const Vec2Double& origin, double& param) const
    {
        auto qa = p0 - origin;
        auto ab = p1 - p0;
        auto br = p0 + p2 - p1 - p1;
        double a = Dot(br, br);
        double b = 3.0 * Dot(ab, br);
        double c = 2.0 * Dot(ab, ab) + Dot(qa, br);
        double d = Dot(qa, ab);
        double t0 = 0;
        double t1 = 0;
        double t2 = 0;
        int solutions = solveCubic(t0, t1, t2, a, b, c, d);

        double minDistance = nonZeroSign(cross(ab, qa)) * Length(qa); // distance from A
        param = -Dot(qa, ab) / Dot(ab, ab);
        {
            double distance = nonZeroSign(cross(p2 - p1, p2 - origin)) * Length(p2 - origin); // distance from B
            if (abs(distance) < abs(minDistance))
            {
                minDistance = distance;
                param = Dot(origin - p1, p2 - p1) / Dot(p2 - p1, p2 - p1);
            }
        }

        if (solutions > 0) param = applySolution(t0, param, ab, br, origin, minDistance);
        if (solutions > 1) param = applySolution(t1, param, ab, br, origin, minDistance);
        if (solutions > 2) param = applySolution(t2, param, ab, br, origin, minDistance);

        if (param >= 0 && param <= 1)
            return SignedDistance(minDistance, 0);
        if (param < .5)
            return SignedDistance(minDistance, Abs(Dot(Normalize(ab), Normalize(qa))));

        return SignedDistance(minDistance, Abs(Dot(Normalize(p2 - p1), Normalize(p2 - origin))));
    }
}
