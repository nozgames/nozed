/*

    NoZ Game Engine

    Copyright(c) 2025 NoZ Games, LLC

*/

#include "SignedDistance.h"
#include <vector>

namespace noz::msdf
{
    enum class EdgeColor
    {
        White
    };

    struct Edge
    {
        Edge(EdgeColor color);

        virtual ~Edge() = default;

        virtual Vec2Double point(double mix) const = 0;
        virtual void splitInThirds(std::vector<Edge*>& result) const = 0;
        virtual void bounds(double& l, double& b, double& r, double& t) const = 0;
        virtual SignedDistance distance(const Vec2Double& origin, double& param) const = 0;

        static void bounds(const Vec2Double& p, double& l, double& b, double& r, double& t);

        EdgeColor color;
    };

    struct LinearEdge : public Edge
    {
        LinearEdge(const Vec2Double& p0, const Vec2Double& p1);
        LinearEdge(const Vec2Double& p0, const Vec2Double& p1, EdgeColor color);

        Vec2Double point(double mix) const override;
        void splitInThirds(std::vector<Edge*>& result) const override;
        void bounds(double& l, double& b, double& r, double& t) const override;
        SignedDistance distance(const Vec2Double& origin, double& param) const override;

        Vec2Double p0;
        Vec2Double p1;
    };

    struct QuadraticEdge : public Edge
    {
        QuadraticEdge(const Vec2Double& p0, const Vec2Double& p1, const Vec2Double& p2);
        QuadraticEdge(const Vec2Double& p0, const Vec2Double& p1, const Vec2Double& p2, EdgeColor color);

        Vec2Double point(double mix) const override;
        void splitInThirds(std::vector<Edge*>& result) const override;
        void bounds(double& l, double& b, double& r, double& t) const override;
        SignedDistance distance(const Vec2Double& origin, double& param) const override;

        Vec2Double p0;
        Vec2Double p1;
        Vec2Double p2;

    private:

        double applySolution(
            double t,
            double oldSolution,
            const Vec2Double& ab,
            const Vec2Double& br,
            const Vec2Double& origin,
            double& minDistance) const;
    };
}
