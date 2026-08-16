#include "util/QrCode.h"

#include <QPainter>

namespace {

// ---- capacity tables for error-correction level M, versions 1..10 ----
struct VersionInfo
{
    int totalCodewords;
    int ecPerBlock;
    int blocks1;
    int data1;      // data codewords in each group-1 block
    int blocks2;
    int data2;      // data codewords in each group-2 block
};

const VersionInfo kVersions[11] = {
    {0, 0, 0, 0, 0, 0},                 // index 0 unused
    {26,  10, 1, 16, 0,  0},            // v1
    {44,  16, 1, 28, 0,  0},            // v2
    {70,  26, 1, 44, 0,  0},            // v3
    {100, 18, 2, 32, 0,  0},            // v4
    {134, 24, 2, 43, 0,  0},            // v5
    {172, 16, 4, 27, 0,  0},            // v6
    {196, 18, 4, 31, 0,  0},            // v7
    {242, 22, 2, 38, 2, 39},            // v8
    {292, 22, 3, 36, 2, 37},            // v9
    {346, 26, 4, 43, 1, 44},            // v10
};

int dataCapacity(int version)
{
    const VersionInfo &v = kVersions[version];
    return v.blocks1 * v.data1 + v.blocks2 * v.data2;
}

// Alignment-pattern centre coordinates per version.
const QVector<int> kAlignment[11] = {
    {},                 // 0
    {},                 // v1 has none
    {6, 18}, {6, 22}, {6, 26}, {6, 30}, {6, 34},
    {6, 22, 38}, {6, 24, 42}, {6, 26, 46}, {6, 28, 50},
};

// ------------------------------------------------------------ GF(256)
quint8 gfExp[512];
quint8 gfLog[256];
bool gfReady = false;

void initGf()
{
    if (gfReady)
        return;
    int x = 1;
    for (int i = 0; i < 255; ++i) {
        gfExp[i] = quint8(x);
        gfLog[x] = quint8(i);
        x <<= 1;
        if (x & 0x100)
            x ^= 0x11D;             // the QR primitive polynomial
    }
    for (int i = 255; i < 512; ++i)
        gfExp[i] = gfExp[i - 255];
    gfReady = true;
}

quint8 gfMul(quint8 a, quint8 b)
{
    if (a == 0 || b == 0)
        return 0;
    return gfExp[gfLog[a] + gfLog[b]];
}

// Generator polynomial for 'degree' error-correction codewords.
QVector<quint8> rsGenerator(int degree)
{
    QVector<quint8> poly{1};
    for (int i = 0; i < degree; ++i) {
        QVector<quint8> next(poly.size() + 1, 0);
        for (int j = 0; j < poly.size(); ++j) {
            next[j] ^= gfMul(poly[j], 1);            // multiply by x
            next[j + 1] ^= gfMul(poly[j], gfExp[i]); // ... and by alpha^i
        }
        poly = next;
    }
    return poly;
}

QVector<quint8> rsRemainder(const QVector<quint8> &data, int ecCount)
{
    const QVector<quint8> gen = rsGenerator(ecCount);
    QVector<quint8> remainder(ecCount, 0);
    for (quint8 byte : data) {
        const quint8 factor = byte ^ remainder[0];
        remainder.removeFirst();
        remainder.append(0);
        for (int i = 0; i < ecCount; ++i)
            remainder[i] ^= gfMul(gen[i + 1], factor);
    }
    return remainder;
}

} // namespace

// ------------------------------------------------------------ encoding

QrCode QrCode::encode(const QByteArray &data)
{
    initGf();
    QrCode code;

    // Byte mode: 4 bits mode + character count + the bytes themselves.
    // Versions 1-9 use an 8-bit count, 10 and up use 16.
    int version = 0;
    for (int v = 1; v <= 10; ++v) {
        const int countBits = (v <= 9) ? 8 : 16;
        const int needed = 4 + countBits + data.size() * 8;
        if (needed <= dataCapacity(v) * 8) {
            version = v;
            break;
        }
    }
    if (version == 0)
        return code;                      // too long for this encoder

    const VersionInfo &info = kVersions[version];
    const int countBits = (version <= 9) ? 8 : 16;

    // ---- bit stream
    QVector<bool> bits;
    auto appendBits = [&bits](quint32 value, int length) {
        for (int i = length - 1; i >= 0; --i)
            bits.append(((value >> i) & 1) != 0);
    };

    appendBits(0b0100, 4);                        // byte mode
    appendBits(quint32(data.size()), countBits);
    for (unsigned char byte : data)
        appendBits(byte, 8);

    const int capacityBits = dataCapacity(version) * 8;
    // Terminator, then pad to a whole byte.
    for (int i = 0; i < 4 && bits.size() < capacityBits; ++i)
        bits.append(false);
    while (bits.size() % 8 != 0)
        bits.append(false);

    QVector<quint8> dataCodewords;
    for (int i = 0; i < bits.size(); i += 8) {
        quint8 byte = 0;
        for (int b = 0; b < 8; ++b)
            byte = quint8((byte << 1) | (bits[i + b] ? 1 : 0));
        dataCodewords.append(byte);
    }
    // Pad bytes alternate between these two values, by specification.
    for (bool alternate = true; dataCodewords.size() < dataCapacity(version);
         alternate = !alternate)
        dataCodewords.append(alternate ? 0xEC : 0x11);

    // ---- split into blocks, compute EC, then interleave
    QVector<QVector<quint8>> dataBlocks;
    QVector<QVector<quint8>> ecBlocks;
    int offset = 0;
    const int totalBlocks = info.blocks1 + info.blocks2;
    for (int b = 0; b < totalBlocks; ++b) {
        const int length = (b < info.blocks1) ? info.data1 : info.data2;
        QVector<quint8> block = dataCodewords.mid(offset, length);
        offset += length;
        dataBlocks.append(block);
        ecBlocks.append(rsRemainder(block, info.ecPerBlock));
    }

    QVector<quint8> finalCodewords;
    const int longestData = qMax(info.data1, info.data2);
    for (int i = 0; i < longestData; ++i) {
        for (const QVector<quint8> &block : dataBlocks) {
            if (i < block.size())
                finalCodewords.append(block[i]);
        }
    }
    for (int i = 0; i < info.ecPerBlock; ++i) {
        for (const QVector<quint8> &block : ecBlocks)
            finalCodewords.append(block[i]);
    }

    // ---- draw
    code.size = 17 + 4 * version;
    code.modules.fill(false, code.size * code.size);
    code.reserved.fill(false, code.size * code.size);
    code.drawFunctionPatterns(version);
    code.drawCodewords(finalCodewords);

    // Pick the mask with the lowest penalty, as the specification requires.
    int bestMask = 0;
    int bestPenalty = -1;
    const QVector<bool> undrawn = code.modules;
    for (int mask = 0; mask < 8; ++mask) {
        code.modules = undrawn;
        code.applyMask(mask);
        code.drawFormatBits(mask);
        const int p = code.penalty();
        if (bestPenalty < 0 || p < bestPenalty) {
            bestPenalty = p;
            bestMask = mask;
        }
    }
    code.modules = undrawn;
    code.applyMask(bestMask);
    code.drawFormatBits(bestMask);

    return code;
}

// ------------------------------------------------------------- drawing

bool QrCode::moduleAt(int x, int y) const
{
    if (x < 0 || y < 0 || x >= size || y >= size)
        return false;
    return modules[y * size + x];
}

void QrCode::setModule(int x, int y, bool dark)
{
    if (x < 0 || y < 0 || x >= size || y >= size)
        return;
    modules[y * size + x] = dark;
    reserved[y * size + x] = true;
}

bool QrCode::reservedAt(int x, int y) const
{
    return reserved[y * size + x];
}

void QrCode::drawFinder(int x, int y)
{
    for (int dy = -1; dy <= 7; ++dy) {
        for (int dx = -1; dx <= 7; ++dx) {
            const int px = x + dx;
            const int py = y + dy;
            if (px < 0 || py < 0 || px >= size || py >= size)
                continue;
            const int ring = qMax(qAbs(dx - 3), qAbs(dy - 3));
            setModule(px, py, ring != 2 && ring <= 3);
        }
    }
}

void QrCode::drawAlignment(int cx, int cy)
{
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx)
            setModule(cx + dx, cy + dy, qMax(qAbs(dx), qAbs(dy)) != 1);
    }
}

void QrCode::drawFunctionPatterns(int version)
{
    drawFinder(0, 0);
    drawFinder(size - 7, 0);
    drawFinder(0, size - 7);

    // Timing patterns.
    for (int i = 8; i < size - 8; ++i) {
        setModule(i, 6, i % 2 == 0);
        setModule(6, i, i % 2 == 0);
    }

    // Alignment patterns, skipping the three finder corners.
    const QVector<int> &centres = kAlignment[version];
    for (int i = 0; i < centres.size(); ++i) {
        for (int j = 0; j < centres.size(); ++j) {
            const bool corner = (i == 0 && j == 0)
                                || (i == 0 && j == centres.size() - 1)
                                || (i == centres.size() - 1 && j == 0);
            if (!corner)
                drawAlignment(centres[i], centres[j]);
        }
    }

    // The always-dark module, and the space the format bits will occupy.
    // Row 6 and column 6 belong to the timing patterns and must survive: the
    // format strip steps over them, so reserving them here would erase two
    // timing modules — which is exactly what it used to do.
    setModule(8, size - 8, true);
    for (int i = 0; i <= 8; ++i) {
        if (i != 6) {
            setModule(i, 8, false);
            setModule(8, i, false);
        }
    }
    for (int i = 0; i < 8; ++i) {
        setModule(size - 1 - i, 8, false);
        setModule(8, size - 1 - i, false);
    }

    if (version >= 7)
        drawVersionBits(version);
}

void QrCode::drawFormatBits(int mask)
{
    // 2 bits for level M (00) + 3 mask bits, extended by a BCH(15,5) code.
    const int data = (0b00 << 3) | mask;
    int rem = data;
    for (int i = 0; i < 10; ++i)
        rem = (rem << 1) ^ ((rem >> 9) * 0x537);
    const int bitsValue = ((data << 10) | rem) ^ 0x5412;

    auto bitAt = [bitsValue](int i) { return ((bitsValue >> i) & 1) != 0; };

    for (int i = 0; i <= 5; ++i)
        setModule(8, i, bitAt(i));
    setModule(8, 7, bitAt(6));
    setModule(8, 8, bitAt(7));
    setModule(7, 8, bitAt(8));
    for (int i = 9; i < 15; ++i)
        setModule(14 - i, 8, bitAt(i));

    for (int i = 0; i < 8; ++i)
        setModule(size - 1 - i, 8, bitAt(i));
    for (int i = 8; i < 15; ++i)
        setModule(8, size - 15 + i, bitAt(i));
    setModule(8, size - 8, true);      // stays dark
}

void QrCode::drawVersionBits(int version)
{
    int rem = version;
    for (int i = 0; i < 12; ++i)
        rem = (rem << 1) ^ ((rem >> 11) * 0x1F25);
    const int bitsValue = (version << 12) | rem;

    for (int i = 0; i < 18; ++i) {
        const bool bit = ((bitsValue >> i) & 1) != 0;
        const int a = size - 11 + i % 3;
        const int b = i / 3;
        setModule(a, b, bit);
        setModule(b, a, bit);
    }
}

void QrCode::drawCodewords(const QVector<quint8> &codewords)
{
    int index = 0;
    // Two columns at a time, right to left, zigzagging up then down.
    for (int right = size - 1; right >= 1; right -= 2) {
        if (right == 6)
            right = 5;                    // column 6 is the timing pattern
        for (int vert = 0; vert < size; ++vert) {
            for (int j = 0; j < 2; ++j) {
                const int x = right - j;
                const bool upward = ((right + 1) & 2) == 0;
                const int y = upward ? size - 1 - vert : vert;
                if (reservedAt(x, y))
                    continue;
                bool dark = false;
                if (index < codewords.size() * 8) {
                    const quint8 byte = codewords[index >> 3];
                    dark = ((byte >> (7 - (index & 7))) & 1) != 0;
                }
                modules[y * size + x] = dark;
                ++index;                  // extra bits past the end stay light
            }
        }
    }
}

void QrCode::applyMask(int mask)
{
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (reservedAt(x, y))
                continue;
            bool invert = false;
            switch (mask) {
            case 0: invert = (x + y) % 2 == 0; break;
            case 1: invert = y % 2 == 0; break;
            case 2: invert = x % 3 == 0; break;
            case 3: invert = (x + y) % 3 == 0; break;
            case 4: invert = (x / 3 + y / 2) % 2 == 0; break;
            case 5: invert = x * y % 2 + x * y % 3 == 0; break;
            case 6: invert = (x * y % 2 + x * y % 3) % 2 == 0; break;
            case 7: invert = ((x + y) % 2 + x * y % 3) % 2 == 0; break;
            }
            if (invert)
                modules[y * size + x] = !modules[y * size + x];
        }
    }
}

int QrCode::penalty() const
{
    int result = 0;

    // Rule 1: every run of five or more identical modules, in each row and
    // each column, costs 3 plus one for each module past the fifth.
    for (int y = 0; y < size; ++y) {
        int run = 1;
        for (int x = 1; x <= size; ++x) {
            if (x < size && moduleAt(x, y) == moduleAt(x - 1, y)) {
                ++run;
            } else {
                if (run >= 5)
                    result += 3 + (run - 5);
                run = 1;
            }
        }
    }
    for (int x = 0; x < size; ++x) {
        int run = 1;
        for (int y = 1; y <= size; ++y) {
            if (y < size && moduleAt(x, y) == moduleAt(x, y - 1)) {
                ++run;
            } else {
                if (run >= 5)
                    result += 3 + (run - 5);
                run = 1;
            }
        }
    }

    // Rule 2: 2x2 blocks of one colour.
    for (int y = 0; y < size - 1; ++y) {
        for (int x = 0; x < size - 1; ++x) {
            const bool c = moduleAt(x, y);
            if (c == moduleAt(x + 1, y) && c == moduleAt(x, y + 1)
                && c == moduleAt(x + 1, y + 1))
                result += 3;
        }
    }

    // Rule 3: the finder-like 1:1:3:1:1 pattern with four light modules beside it.
    const bool patternA[11] = {true, false, true, true, true, false, true,
                               false, false, false, false};
    const bool patternB[11] = {false, false, false, false, true, false, true,
                               true, true, false, true};
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x <= size - 11; ++x) {
            bool matchA = true, matchB = true;
            for (int i = 0; i < 11; ++i) {
                if (moduleAt(x + i, y) != patternA[i]) matchA = false;
                if (moduleAt(x + i, y) != patternB[i]) matchB = false;
            }
            if (matchA || matchB)
                result += 40;
        }
    }
    for (int x = 0; x < size; ++x) {
        for (int y = 0; y <= size - 11; ++y) {
            bool matchA = true, matchB = true;
            for (int i = 0; i < 11; ++i) {
                if (moduleAt(x, y + i) != patternA[i]) matchA = false;
                if (moduleAt(x, y + i) != patternB[i]) matchB = false;
            }
            if (matchA || matchB)
                result += 40;
        }
    }

    // Rule 4: how far the dark/light balance strays from 50%.
    int dark = 0;
    for (bool m : modules) {
        if (m)
            ++dark;
    }
    const int total = size * size;
    const int percent = dark * 100 / total;
    const int deviation = qAbs(percent - 50) / 5;
    result += deviation * 10;

    return result;
}

QImage QrCode::toImage(int pixelsPerModule, int quietZone) const
{
    if (!isValid())
        return {};

    const int side = (size + 2 * quietZone) * pixelsPerModule;
    QImage image(side, side, QImage::Format_RGB32);
    image.fill(Qt::white);

    QPainter painter(&image);
    painter.setPen(Qt::NoPen);
    painter.setBrush(Qt::black);
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            if (moduleAt(x, y)) {
                painter.drawRect((x + quietZone) * pixelsPerModule,
                                 (y + quietZone) * pixelsPerModule,
                                 pixelsPerModule, pixelsPerModule);
            }
        }
    }
    painter.end();
    return image;
}
