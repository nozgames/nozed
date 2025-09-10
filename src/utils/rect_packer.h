//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#pragma once

#include <vector>

namespace noz 
{
    class rect_packer 
    {
    public: 
        
        enum class method 
        {
            BestShortSideFit,   ///< -BSSF: Positions the rectangle against the short side of a free rectangle into which it fits the best.
            BestLongSideFit,    ///< -BLSF: Positions the rectangle against the long side of a free rectangle into which it fits the best.
            BestAreaFit,        ///< -BAF: Positions the rectangle into the smallest free rect into which it fits.
            BottomLeftRule,     ///< -BL: Does the Tetris placement.
            ContactPointRule    ///< -CP: Chooses the placement where the rectangle touches other rects as much as possible.
        };

        struct BinSize
        {
            BinSize(void) { w = 0; h = 0; }
            BinSize(int32_t _w, int32_t _h) { w = _w; h = _h; }

            int32_t w;
            int32_t h;
        };

        struct BinRect 
        {
            BinRect(void) { x = y = w = h = 0; }
            BinRect(int32_t _x, int32_t _y, int32_t _w, int32_t _h) { x = _x; y = _y; w = _w; h = _h; }

            int32_t x;
            int32_t y;
            int32_t w;
            int32_t h;
        };

        rect_packer(void);
        rect_packer(int32_t width, int32_t height);

        void Resize(int32_t width, int32_t height);

        int Insert(const Vec2Int& size, method method, BinRect& result);
        int Insert(int32_t width, int32_t height, method method, BinRect& result) { return Insert(Vec2Int(width, height), method, result); }

        float GetOccupancy(void) const;

        const BinSize& size(void) const { return size_; }

        bool empty() const { return used_.empty(); }

        bool validate() const;

    private:

        BinRect ScoreRect(BinSize size, method method, int32_t& score1, int32_t& score2) const;

        void PlaceRect(const BinRect& node);

        int32_t ContactPointScoreNode(int x, int y, int width, int height) const;

        BinRect FindPositionForNewNodeBottomLeft(int width, int height, int& bestY, int& bestX) const;
        BinRect FindPositionForNewNodeBestShortSideFit(int width, int height, int& bestShortSideFit, int& bestLongSideFit) const;
        BinRect FindPositionForNewNodeBestLongSideFit(int width, int height, int& bestShortSideFit, int& bestLongSideFit) const;
        BinRect FindPositionForNewNodeBestAreaFit(int width, int height, int& bestAreaFit, int& bestShortSideFit) const;
        BinRect FindPositionForNewNodeContactPoint(int width, int height, int& contactScore) const;

        bool SplitFreeNode(BinRect freeRect, const BinRect& usedRect);

        void PruneFreeList(void);

        bool IsContainedIn(const BinRect& a, const BinRect& b) const 
        {
            return a.x >= b.x && a.y >= b.y && a.x + a.w <= b.x + b.w && a.y + a.h <= b.y + b.h;
        }

        BinSize size_;
        std::vector<BinRect> used_;
        std::vector<BinRect> free_;
    };
}
