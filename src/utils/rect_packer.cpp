//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "rect_packer.h"

using namespace noz;

rect_packer::rect_packer()
{
}

rect_packer::rect_packer(int32_t width, int32_t height)
{
    Resize(width, height);
}

void rect_packer::Resize(int32_t width, int32_t height)
{
    size_.w = width;
    size_.h = height;

    used_.clear();
    free_.clear();
    free_.push_back(BinRect(1, 1, width - 2, height - 2));
}

int rect_packer::Insert(const Vec2Int& size, method method, BinRect& result)
{
    BinRect rect;
    int32_t score1;
    int32_t score2;

    switch (method)
    {
    case method::BestShortSideFit:
        rect = FindPositionForNewNodeBestShortSideFit(size.x, size.y, score1, score2);
        break;
    case method::BottomLeftRule:
        rect = FindPositionForNewNodeBottomLeft(size.x, size.y, score1, score2);
        break;
    case method::ContactPointRule:
        rect = FindPositionForNewNodeContactPoint(size.x, size.y, score1);
        break;
    case method::BestLongSideFit:
        rect = FindPositionForNewNodeBestLongSideFit(size.x, size.y, score2, score1);
        break;
    case method::BestAreaFit:
        rect = FindPositionForNewNodeBestAreaFit(size.x, size.y, score1, score2);
        break;
    }

    if (rect.h == 0)
        return -1;

    PlaceRect(rect);

    result = rect;

    return (int)used_.size() - 1;
}

void rect_packer::PlaceRect(const BinRect& rect)
{
    size_t freeCount = free_.size();
    for (size_t i = 0; i < freeCount; ++i)
    {
        if (SplitFreeNode(free_[i], rect))
        {
            free_.erase(free_.begin() + i);
            --i;
            --freeCount;
        }
    }

    PruneFreeList();

    used_.push_back(rect);
}

rect_packer::BinRect rect_packer::ScoreRect(BinSize size, method method, int32_t& score1, int32_t& score2) const
{
    BinRect rect;
    score1 = std::numeric_limits<int32_t>::max();
    score2 = std::numeric_limits<int32_t>::max();

    switch (method)
    {
    case method::BestShortSideFit:
        rect = FindPositionForNewNodeBestShortSideFit(size.w, size.h, score1, score2);
        break;
    case method::BottomLeftRule:
        rect = FindPositionForNewNodeBottomLeft(size.w, size.h, score1, score2);
        break;
    case method::ContactPointRule:
        rect = FindPositionForNewNodeContactPoint(size.w, size.h, score1);
        score1 = -score1; // Reverse since we are minimizing, but for contact point score bigger is better.
        break;
    case method::BestLongSideFit:
        rect = FindPositionForNewNodeBestLongSideFit(size.w, size.h, score2, score1);
        break;
    case method::BestAreaFit:
        rect = FindPositionForNewNodeBestAreaFit(size.w, size.h, score1, score2);
        break;
    }

    // Cannot fit the current rectangle.
    if (rect.h == 0)
    {
        score1 = std::numeric_limits<int32_t>::max();
        score2 = std::numeric_limits<int32_t>::max();
    }

    return rect;
}

/// Computes the ratio of used surface area.
float rect_packer::GetOccupancy() const
{
    unsigned long area = 0;
    for (size_t i = 0; i < used_.size(); ++i)
    {
        area += used_[i].w * used_[i].h;
    }

    return (float)area / (size_.w * size_.h);
}

rect_packer::BinRect rect_packer::FindPositionForNewNodeBottomLeft(int32_t width, int32_t height, int32_t& bestY,
                                                                   int32_t& bestX) const
{
    BinRect rect;

    bestY = std::numeric_limits<int32_t>::max();

    for (size_t i = 0; i < free_.size(); ++i)
    {
        // Try to place the rectangle in upright (non-flipped) orientation.
        if (free_[i].w >= width && free_[i].h >= height)
        {
            int32_t topSideY = free_[i].y + height;
            if (topSideY < bestY || (topSideY == bestY && free_[i].x < bestX))
            {
                rect.x = free_[i].x;
                rect.y = free_[i].y;
                rect.w = width;
                rect.h = height;
                bestY = topSideY;
                bestX = free_[i].x;
            }
        }
        if (free_[i].w >= height && free_[i].h >= width)
        {
            int32_t topSideY = free_[i].y + width;
            if (topSideY < bestY || (topSideY == bestY && free_[i].x < bestX))
            {
                rect.x = free_[i].x;
                rect.y = free_[i].y;
                rect.w = width;
                rect.h = height;
                bestY = topSideY;
                bestX = free_[i].x;
            }
        }
    }
    return rect;
}

rect_packer::BinRect rect_packer::FindPositionForNewNodeBestShortSideFit(int32_t width, int32_t height,
                                                                         int32_t& bestShortSideFit,
                                                                         int32_t& bestLongSideFit) const
{
    BinRect rect;
    memset(&rect, 0, sizeof(rect));

    bestShortSideFit = std::numeric_limits<int32_t>::max();

    for (size_t i = 0; i < free_.size(); ++i)
    {
        // Try to place the rectangle in upright (non-flipped) orientation.
        if (free_[i].w >= width && free_[i].h >= height)
        {
            int32_t leftoverHoriz = abs(free_[i].w - width);
            int32_t leftoverVert = abs(free_[i].h - height);
            int32_t shortSideFit = std::min(leftoverHoriz, leftoverVert);
            int32_t longSideFit = std::max(leftoverHoriz, leftoverVert);

            if (shortSideFit < bestShortSideFit || (shortSideFit == bestShortSideFit && longSideFit < bestLongSideFit))
            {
                rect.x = free_[i].x;
                rect.y = free_[i].y;
                rect.w = width;
                rect.h = height;
                bestShortSideFit = shortSideFit;
                bestLongSideFit = longSideFit;
            }
        }

        if (free_[i].w >= height && free_[i].h >= width)
        {
            int32_t flippedLeftoverHoriz = abs(free_[i].w - height);
            int32_t flippedLeftoverVert = abs(free_[i].h - width);
            int32_t flippedShortSideFit = std::min(flippedLeftoverHoriz, flippedLeftoverVert);
            int32_t flippedLongSideFit = std::max(flippedLeftoverHoriz, flippedLeftoverVert);

            if (flippedShortSideFit < bestShortSideFit ||
                (flippedShortSideFit == bestShortSideFit && flippedLongSideFit < bestLongSideFit))
            {
                rect.x = free_[i].x;
                rect.y = free_[i].y;
                rect.w = height;
                rect.h = width;
                bestShortSideFit = flippedShortSideFit;
                bestLongSideFit = flippedLongSideFit;
            }
        }
    }
    return rect;
}

rect_packer::BinRect rect_packer::FindPositionForNewNodeBestLongSideFit(int32_t width, int32_t height,
                                                                        int32_t& bestShortSideFit,
                                                                        int32_t& bestLongSideFit) const
{
    BinRect rect;
    memset(&rect, 0, sizeof(rect));

    bestLongSideFit = std::numeric_limits<int32_t>::max();

    for (size_t i = 0; i < free_.size(); ++i)
    {
        // Try to place the rectangle in upright (non-flipped) orientation.
        if (free_[i].w >= width && free_[i].h >= height)
        {
            int32_t leftoverHoriz = abs(free_[i].w - width);
            int32_t leftoverVert = abs(free_[i].h - height);
            int32_t shortSideFit = std::min(leftoverHoriz, leftoverVert);
            int32_t longSideFit = std::max(leftoverHoriz, leftoverVert);

            if (longSideFit < bestLongSideFit || (longSideFit == bestLongSideFit && shortSideFit < bestShortSideFit))
            {
                rect.x = free_[i].x;
                rect.y = free_[i].y;
                rect.w = width;
                rect.h = height;
                bestShortSideFit = shortSideFit;
                bestLongSideFit = longSideFit;
            }
        }
        /*
            if (free_[i].w >= height && free_[i].h >= width)
            {
                int32_t leftoverHoriz = abs(free_[i].w - height);
                int32_t leftoverVert = abs(free_[i].h - width);
                int32_t shortSideFit = math::min(leftoverHoriz, leftoverVert);
                int32_t longSideFit = math::max(leftoverHoriz, leftoverVert);

                if (longSideFit < bestLongSideFit || (longSideFit == bestLongSideFit && shortSideFit <
           bestShortSideFit))
                {
                    rect.x = free_[i].x;
                    rect.y = free_[i].y;
                    rect.w = height;
                    rect.h = width;
                    bestShortSideFit = shortSideFit;
                    bestLongSideFit = longSideFit;
                }
            }
    */
    }
    return rect;
}

rect_packer::BinRect rect_packer::FindPositionForNewNodeBestAreaFit(int32_t width, int32_t height, int32_t& bestAreaFit,
                                                                    int32_t& bestShortSideFit) const
{
    BinRect rect;
    memset(&rect, 0, sizeof(rect));

    bestAreaFit = std::numeric_limits<int32_t>::max();

    for (size_t i = 0; i < free_.size(); ++i)
    {
        int32_t areaFit = free_[i].w * free_[i].h - width * height;

        // Try to place the rectangle in upright (non-flipped) orientation.
        if (free_[i].w >= width && free_[i].h >= height)
        {
            int32_t leftoverHoriz = abs(free_[i].w - width);
            int32_t leftoverVert = abs(free_[i].h - height);
            int32_t shortSideFit = std::min(leftoverHoriz, leftoverVert);

            if (areaFit < bestAreaFit || (areaFit == bestAreaFit && shortSideFit < bestShortSideFit))
            {
                rect.x = free_[i].x;
                rect.y = free_[i].y;
                rect.w = width;
                rect.h = height;
                bestShortSideFit = shortSideFit;
                bestAreaFit = areaFit;
            }
        }

        if (free_[i].w >= height && free_[i].h >= width)
        {
            int32_t leftoverHoriz = abs(free_[i].w - height);
            int32_t leftoverVert = abs(free_[i].h - width);
            int32_t shortSideFit = std::min(leftoverHoriz, leftoverVert);

            if (areaFit < bestAreaFit || (areaFit == bestAreaFit && shortSideFit < bestShortSideFit))
            {
                rect.x = free_[i].x;
                rect.y = free_[i].y;
                rect.w = height;
                rect.h = width;
                bestShortSideFit = shortSideFit;
                bestAreaFit = areaFit;
            }
        }
    }
    return rect;
}

/// Returns 0 if the two intervals i1 and i2 are disjoint, or the length of their overlap otherwise.
int32_t CommonIntervalLength(int32_t i1start, int32_t i1end, int32_t i2start, int32_t i2end)
{
    if (i1end < i2start || i2end < i1start)
        return 0;
    return std::min(i1end, i2end) - std::max(i1start, i2start);
}

int32_t rect_packer::ContactPointScoreNode(int32_t x, int32_t y, int32_t width, int32_t height) const
{
    int32_t score = 0;

    if (x == 0 || x + width == size_.w)
        score += height;
    if (y == 0 || y + height == size_.h)
        score += width;

    for (size_t i = 0; i < used_.size(); ++i)
    {
        if (used_[i].x == x + width || used_[i].x + used_[i].w == x)
            score += CommonIntervalLength(used_[i].y, used_[i].y + used_[i].h, y, y + height);
        if (used_[i].y == y + height || used_[i].y + used_[i].h == y)
            score += CommonIntervalLength(used_[i].x, used_[i].x + used_[i].w, x, x + width);
    }

    return score;
}

rect_packer::BinRect rect_packer::FindPositionForNewNodeContactPoint(int32_t width, int32_t height,
                                                                     int32_t& bestContactScore) const
{
    BinRect rect;
    memset(&rect, 0, sizeof(rect));

    bestContactScore = -1;

    for (size_t i = 0; i < free_.size(); ++i)
    {
        // Try to place the rectangle in upright (non-flipped) orientation.
        if (free_[i].w >= width && free_[i].h >= height)
        {
            int32_t score = ContactPointScoreNode(free_[i].x, free_[i].y, width, height);
            if (score > bestContactScore)
            {
                rect.x = free_[i].x;
                rect.y = free_[i].y;
                rect.w = width;
                rect.h = height;
                bestContactScore = score;
            }
        }
        if (free_[i].w >= height && free_[i].h >= width)
        {
            int32_t score = ContactPointScoreNode(free_[i].x, free_[i].y, width, height);
            if (score > bestContactScore)
            {
                rect.x = free_[i].x;
                rect.y = free_[i].y;
                rect.w = height;
                rect.h = width;
                bestContactScore = score;
            }
        }
    }
    return rect;
}

bool rect_packer::SplitFreeNode(BinRect freeNode, const BinRect& usedNode)
{
    // Test with SAT if the rectangles even intersect.
    if (usedNode.x >= freeNode.x + freeNode.w || usedNode.x + usedNode.w <= freeNode.x ||
        usedNode.y >= freeNode.y + freeNode.h || usedNode.y + usedNode.h <= freeNode.y)
        return false;

    if (usedNode.x < freeNode.x + freeNode.w && usedNode.x + usedNode.w > freeNode.x)
    {
        // New node at the top side of the used node.
        if (usedNode.y > freeNode.y && usedNode.y < freeNode.y + freeNode.h)
        {
            BinRect newNode = freeNode;
            newNode.h = usedNode.y - newNode.y;
            free_.push_back(newNode);
        }

        // New node at the bottom side of the used node.
        if (usedNode.y + usedNode.h < freeNode.y + freeNode.h)
        {
            BinRect newNode = freeNode;
            newNode.y = usedNode.y + usedNode.h;
            newNode.h = freeNode.y + freeNode.h - (usedNode.y + usedNode.h);
            free_.push_back(newNode);
        }
    }

    if (usedNode.y < freeNode.y + freeNode.h && usedNode.y + usedNode.h > freeNode.y)
    {
        // New node at the left side of the used node.
        if (usedNode.x > freeNode.x && usedNode.x < freeNode.x + freeNode.w)
        {
            BinRect newNode = freeNode;
            newNode.w = usedNode.x - newNode.x;
            free_.push_back(newNode);
        }

        // New node at the right side of the used node.
        if (usedNode.x + usedNode.w < freeNode.x + freeNode.w)
        {
            BinRect newNode = freeNode;
            newNode.x = usedNode.x + usedNode.w;
            newNode.w = freeNode.x + freeNode.w - (usedNode.x + usedNode.w);
            free_.push_back(newNode);
        }
    }

    return true;
}

void rect_packer::PruneFreeList()
{
    // Remoe redundance rectangles
    for (size_t i = 0; i < free_.size(); ++i)
    {
        for (size_t j = i + 1; j < free_.size(); ++j)
        {
            if (IsContainedIn(free_[i], free_[j]))
            {
                free_.erase(free_.begin() + i);
                --i;
                break;
            }
            if (IsContainedIn(free_[j], free_[i]))
            {
                free_.erase(free_.begin() + j);
                --j;
            }
        }
    }
}

bool rect_packer::validate() const
{
    for (auto& rect : used_)
    {
        if (rect.x < 1 || rect.y < 1 || rect.x + rect.w > size_.w - 1 || rect.y + rect.h > size_.h - 1)
            return false;
    }

    return true;
}
