//
//  NoZ Game Engine - Copyright(c) 2025 NoZ Games, LLC
//

#include "TrueTypeFont.h"
#include <noz/stream.h>

namespace noz::ttf
{
    class TrueTypeFontReader
    {
    public:

        TrueTypeFontReader(Stream* stream, int requestedSize, const std::string& filter);

        TrueTypeFont* read();

    private:

        enum class TableName : uint8_t
        {
            None,
            HEAD,
            LOCA,
            GLYF,
            HMTX,
            HHEA,
            CMAP,
            MAXP,
            KERN,
            Count
        };

        enum class PointFlags : uint8_t
        {
            OnCurve = 1,
            XShortVector = 2,
            YShortVector = 4,
            Repeat = 8,
            XIsSame = 16,
            YIsSame = 32
        };

        float readFixed();
        double readFUnit();
        double readUFUnit();
        std::string readString(int length);
        void readDate();
        uint16_t readUInt16();
        int16_t readInt16();
        uint32_t readUInt32();
        int32_t readInt32();
        std::vector<uint16_t> readUInt16Array(int length);

        void readCMAP();
        void readHEAD();
        void readGlyphs();
        void readPoints(TrueTypeFont::Glyph& glyph, const std::vector<PointFlags>& flags, bool isX);
        void readGlyph(TrueTypeFont::Glyph& glyph);
        void readHHEA();
        void readMAXP();
        void readKERN();

        int64_t seek(int64_t offset);
        int64_t seek(TableName table);
        int64_t seek(TableName table, int64_t offset);

        bool isInFilter(char c) const;

        uint32_t calculateChecksum(uint32_t offset, uint32_t length);

        Stream* _reader;
        TrueTypeFont* _ttf;
        uint16_t _indexToLocFormat;
        std::vector<int64_t> _tableOffsets;
        Vec2Double _scale;
        std::string _filter;
        double _unitsPerEm;
        int _requestedSize;
        std::vector<TrueTypeFont::Glyph*> _glyphsById;
    };
}
