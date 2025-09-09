//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "TrueTypeFontReader.h"

using namespace std;

namespace noz::ttf
{
    constexpr double Fixed = 1.0 / (1 << 16);

    TrueTypeFontReader::TrueTypeFontReader(Stream* reader, int requestedSize, const string& filter)
        : _reader(reader),
        _requestedSize(requestedSize),
        _filter(filter),
        _ttf(nullptr),
        _scale(1.0, 1.0),
        _unitsPerEm(0.0),
        _indexToLocFormat(0)
    {
        _tableOffsets.resize((size_t)TableName::Count);
    }

    bool TrueTypeFontReader::isInFilter(char c) const
    {
        return _filter.find(c) != -1;
    }

    float TrueTypeFontReader::readFixed()
    {
        return (float)(readInt32() * Fixed);
    }

    double TrueTypeFontReader::readFUnit()
    {
        return readInt16() * _scale.x;
    }

    double TrueTypeFontReader::readUFUnit()
    {
        return readUInt16() * _scale.x;
    }

    std::string TrueTypeFontReader::readString(int length)
    {
        std::vector<char> bytes(length);
        ReadBytes(_reader, bytes.data(), length);
        return std::string(bytes.begin(), bytes.end());
    }

    void TrueTypeFontReader::readDate()
    {
        readUInt32();
        readUInt32();
    }

    uint16_t TrueTypeFontReader::readUInt16()
    {
        return (uint16_t)((ReadU8(_reader) << 8) | ReadU8(_reader));
    }

    int16_t TrueTypeFontReader::readInt16()
    {
        return (int16_t)((ReadU8(_reader) << 8) | ReadU8(_reader));
    }

    uint32_t TrueTypeFontReader::readUInt32()
    {
        return
            (((uint32_t)ReadU8(_reader)) << 24) |
            (((uint32_t)ReadU8(_reader)) << 16) |
            (((uint32_t)ReadU8(_reader)) << 8) |
            ReadU8(_reader);
    }

    int32_t TrueTypeFontReader::readInt32()
    {
        return
            ((ReadU8(_reader)) << 24) |
            ((ReadU8(_reader)) << 16) |
            ((ReadU8(_reader)) << 8) |
            ReadU8(_reader);
    }

    std::vector<uint16_t> TrueTypeFontReader::readUInt16Array(int length)
    {
        std::vector<uint16_t> result;
        result.resize(length);
        for (int i = 0; i < length; i++)
            result[i] = readUInt16();
        return result;
    }

    int64_t TrueTypeFontReader::seek(int64_t offset)
    {
        int64_t old = (int64_t)GetPosition(_reader);
        SetPosition(_reader, (size_t)offset);
        return old;
    }

    int64_t TrueTypeFontReader::seek(TableName table)
    {
        return seek(table, 0);
    }

    int64_t TrueTypeFontReader::seek(TableName table, int64_t offset)
    {
        return seek(_tableOffsets[(int)table] + offset);
    }

    uint32_t TrueTypeFontReader::calculateChecksum(uint32_t offset, uint32_t length)
    {
        auto old = seek((int64_t)offset);
        uint32_t sum = 0;
        uint32_t count = (length + 3) / 4;
        for (uint32_t j = 0; j < count; j++)
            sum = (sum + readUInt32() & 0xffffffff);

        seek(old);
        return sum;
    }

    void TrueTypeFontReader::readCMAP()
    {
        seek(TableName::CMAP, 0);

        /*auto version = */
        readUInt16();
        auto tableCount = readUInt16();

        uint32_t offset = 0;
        for (int i = 0; i < tableCount && offset == 0; i++)
        {
            auto platformId = readUInt16();
            auto platformSpecificId = readUInt16();
            auto platformOffset = readUInt32();    // Offset

            if (platformId == 0 || (platformId == 3 && platformSpecificId == 1))
                offset = platformOffset;
        }

        if (offset == 0)
            throw std::invalid_argument("TTF file has no unicode character map.");

        // Seek to the character map 
        seek(TableName::CMAP, offset);

        auto format = readUInt16();
        auto length = readUInt16();
        auto language = readUInt16();

        switch (format)
        {
            case 4:
            {
                auto segcount = readUInt16() / 2;
                auto searchRange = readUInt16();
                auto entitySelector = readUInt16();
                auto rangeShift = readUInt16();
                auto endCode = readUInt16Array(segcount);
                readUInt16();
                auto startCode = readUInt16Array(segcount);
                auto idDelta = readUInt16Array(segcount);
                auto glyphIdArray = GetPosition(_reader);
                auto idRangeOffset = readUInt16Array(segcount);

                for (int i = 0; endCode[i] != 0xFFFF; i++)
                {
                    auto end = endCode[i];
                    auto start = startCode[i];
                    auto delta = (short)idDelta[i];
                    auto rangeOffset = idRangeOffset[i];

                    if (start > 254)
                        continue;
                    if (end > 254)
                        end = 254;

                    if (rangeOffset == 0)
                    {
                        for (int c = start; c <= end; c++)
                        {
                            if (!isInFilter((char)c))
                                continue;

                            auto glyphId = (uint16_t)(c + delta);
                            if (_ttf->_glyphs[c] != nullptr)
                                throw std::exception("Multiple definitions for glyph");

                            auto glyph = new TrueTypeFont::Glyph {};
                            glyph->id = glyphId;
                            glyph->ascii = (char)c;
                            _ttf->_glyphs[c] = glyph;
                            _glyphsById[glyphId] = glyph;
                        }
                    }
                    else
                    {
                        for (int c = start; c <= end; c++)
                        {
                            if (!isInFilter((char)c))
                                continue;

                            seek(glyphIdArray + i * 2 + rangeOffset + 2 * (c - start));
                            auto glyphId = readUInt16();
                            if (_ttf->_glyphs[c] != nullptr)
                                throw std::exception("Multiple definitions for glyph");

                            auto glyph = new TrueTypeFont::Glyph{};
                            glyph->id = glyphId;
                            glyph->ascii = (char)c;
                            _ttf->_glyphs[c] = glyph;
                            _glyphsById[glyphId] = glyph;
                        }
                    }
                }
                break;
            }

            default:
                throw std::exception("not implemented");

        }
    }

    void TrueTypeFontReader::readHEAD()
    {
        seek(TableName::HEAD, 0);

        /* auto version = */
        readFixed();
        /* auto fontRevision = */
        readFixed();
        /* auto checksumAdjustment = */
        readUInt32();
        /* auto magicNumber = */
        readUInt32();
        /* auto flags = */
        readUInt16();
        _unitsPerEm = readUInt16();
        readDate();
        readDate();
        /* auto xmin = */
        readInt16();
        /* auto ymin = */
        readInt16();
        /* auto xmax = */
        readInt16();
        /* auto ymax = */
        readInt16();
        readUInt16();
        readUInt16();
        readInt16();

        _indexToLocFormat = readInt16();

        _scale = {0.0, 0.0};
        _scale.x = _requestedSize / _unitsPerEm;
        _scale.y = _requestedSize / _unitsPerEm;
    }

    void TrueTypeFontReader::readGlyphs()
    {
        for (size_t i = 0; i < _ttf->_glyphs.size(); i++)
        {
            auto glyph = _ttf->_glyphs[i];
            if (glyph == nullptr)
                continue;

            if (glyph->ascii == 58)
                printf("test\n");


            // Seek to the glyph in the GLYF table
            if (_indexToLocFormat == 1)
            {
                seek(TableName::LOCA, glyph->id * 4);
                auto offset = readUInt32();
                auto length = readUInt32() - offset;

                // Empty glyph
                if (length == 0)
                    continue;

                seek(TableName::GLYF, offset);
            }
            else
            {
                seek(TableName::LOCA, glyph->id * 2);

                auto offset = readUInt16() * 2;
                auto length = (readUInt16() * 2) - offset;

                if (length == 0)
                    continue;

                seek(TableName::GLYF, offset);
            }

            // Read the glyph
            readGlyph(*glyph);
        }
    }

    void TrueTypeFontReader::readPoints(TrueTypeFont::Glyph& glyph, const std::vector<PointFlags>& flags, bool isX)
    {
        auto byteFlag = (uint8_t)(isX ? PointFlags::XShortVector : PointFlags::YShortVector);
        auto deltaFlag = (uint8_t)(isX ? PointFlags::XIsSame : PointFlags::YIsSame);

        double value = 0;
        for (int i = 0; i < glyph.points.size(); i++)
        {
            auto& point = glyph.points[i];
            auto pointFlags = (uint8_t)flags[i];

            if ((pointFlags & byteFlag) == byteFlag)
            {
                if ((pointFlags & deltaFlag) == deltaFlag)
                {
                    value += ReadU8(_reader);
                }
                else
                {
                    value -= ReadU8(_reader);
                }
            }
            else if ((pointFlags & deltaFlag) != deltaFlag)
            {
                value += readInt16();
            }

            if (isX)
                point.xy.x = value * _scale.x;
            else
                point.xy.y = value * _scale.y;
        }
    }

    void TrueTypeFontReader::readGlyph(TrueTypeFont::Glyph& glyph)
    {
        int16_t numberOfContours = readInt16();

        // Simple ?
        if (numberOfContours < 0)
            return;

        double minx = readFUnit();
        double miny = readFUnit();
        double maxx = readFUnit();
        double maxy = readFUnit();

        auto endPoints = readUInt16Array(numberOfContours);
        auto instructionLength = readUInt16();
        std::vector<uint8_t> instructions(instructionLength);
        ReadBytes(_reader, instructions.data(), instructionLength);
        auto numPoints = endPoints[endPoints.size() - 1] + 1;

        glyph.contours.resize(numberOfContours);
        for (int i = 0, start = 0; i < numberOfContours; i++)
        {
            glyph.contours[i].start = start;
            glyph.contours[i].length = endPoints[i] - start + 1;
            start = endPoints[i] + 1;
        }

        // Read the flags.
        std::vector<PointFlags> flags;
        flags.resize(numPoints);
        for (int i = 0; i < numPoints;)
        {
            auto readFlags = ReadU8(_reader);
            flags[i++] = (PointFlags)readFlags;

            if ((readFlags & ((uint8_t)PointFlags::Repeat)) == ((uint8_t)PointFlags::Repeat))
            {
                auto repeat = ReadU8(_reader);
                for (int r = 0; r < repeat; r++)
                    flags[i++] = (PointFlags)readFlags;
            }
        }

        glyph.points.resize(numPoints);
        glyph.size = {maxx - minx, maxy - miny};
        glyph.bearing = {minx, maxy};

        for (int i = 0; i < numPoints; i++)
        {
            auto onCurve = (uint8_t(flags[i]) & uint8_t(PointFlags::OnCurve)) == uint8_t(PointFlags::OnCurve);
            glyph.points[i].curve = onCurve
                ? TrueTypeFont::CurveType::None
                : TrueTypeFont::CurveType::Conic;
            glyph.points[i].xy = Vec2Double(0.0);
        }

        readPoints(glyph, flags, true);
        readPoints(glyph, flags, false);
    }

    void TrueTypeFontReader::readHHEA()
    {
        seek(TableName::HHEA);

        /* float verison = */
        readFixed();
        _ttf->_ascent = readFUnit();
        _ttf->_descent = readFUnit();
        _ttf->_height = _ttf->_ascent - _ttf->_descent;

        // Skip
        seek(TableName::HHEA, 34);

        auto metricCount = readUInt16();

        for (int i = 0; i < _ttf->_glyphs.size(); i++)
        {
            auto glyph = _ttf->_glyphs[i];
            if (glyph == nullptr)
                continue;

            // If the glyph is past the end of the total number of metrics
            // then it is contained in the end run..
            if (glyph->id >= metricCount)
                // TODO: implement end run..
                throw std::exception("not implemented");

            seek(TableName::HMTX, glyph->id * 4);

            glyph->advance = readUFUnit();
            double leftBearing = readFUnit();
        }
    }

    void TrueTypeFontReader::readMAXP()
    {
        seek(TableName::MAXP, 0);
        readFixed();
        _glyphsById.resize(readUInt16());
    }

    void TrueTypeFontReader::readKERN()
    {
        seek(TableName::KERN, 2);
        int numTables = readInt16();
        for (int i = 0; i < numTables; i++)
        {
            long tableStart = (long)GetPosition(_reader);
            /*auto version = */
            readUInt16();
            int length = readUInt16();
            int coverage = readUInt16();
            int format = coverage & 0xFF00;

            switch (format)
            {
                case 0:
                {
                    int pairCount = readUInt16();
                    int searchRange = readUInt16();
                    int entrySelector = readUInt16();
                    int rangeShift = readUInt16();

                    for (int pair = 0; pair < pairCount; ++pair)
                    {
                        auto leftId = readUInt16();
                        auto rightId = readUInt16();
                        auto left = _glyphsById[leftId];
                        auto right = _glyphsById[rightId];
                        auto kern = readFUnit();

                        if (left == nullptr || right == nullptr)
                            continue;

                        TrueTypeFont::Kerning kerning = {};
                        kerning.left = left->ascii;
                        kerning.right = right->ascii;
                        kerning.value = (float)kern;
                        _ttf->_kerning.push_back(kerning);
                    }

                    break;
                }

                default:
                    throw std::exception("not implemented");
            }

            seek(tableStart + length);
        }
    }

    TrueTypeFont* TrueTypeFontReader::read()
    {
        _ttf = new TrueTypeFont();

        readUInt32(); // Scalar type
        uint16_t numTables = readUInt16();
        readUInt16(); // Search range
        readUInt16(); // Entry Selector
        readUInt16(); // Range Shift

        // Right now we only support ASCII table.
        _ttf->_glyphs.resize(255, nullptr);

        // Read all of the relevant table offsets and validate their checksums
        for (uint16_t i = 0; i < numTables; i++)
        {
            auto tag = readString(4);
            auto checksum = readUInt32();
            auto offset = readUInt32();
            auto length = readUInt32();

            std::transform(tag.begin(), tag.end(), tag.begin(), ::tolower);

            auto name = TableName::None;

            if (tag == "head")
                name = TableName::HEAD;
            else if (tag == "loca")
                name = TableName::LOCA;
            else if (tag == "glyf")
                name = TableName::GLYF;
            else if (tag == "hmtx")
                name = TableName::HMTX;
            else if (tag == "hhea")
                name = TableName::HHEA;
            else if (tag == "cmap")
                name = TableName::CMAP;
            else if (tag == "maxp")
                name = TableName::MAXP;
            else if (tag == "kern")
                name = TableName::KERN;
            else
                continue;

            _tableOffsets[(int)name] = offset;

            if (tag != "head" && calculateChecksum(offset, length) != checksum)
                throw std::exception("Checksum mismatch");
        }

        readHEAD();
        readMAXP();
        readCMAP();
        readHHEA();
        readGlyphs();
        readKERN();

        return _ttf;
    }
}