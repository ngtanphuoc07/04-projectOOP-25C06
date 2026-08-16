#ifndef QRCODE_H
#define QRCODE_H

#include <QImage>
#include <QString>
#include <QVector>

// A small QR Code encoder — byte mode, error-correction level M, versions 1-10.
//
// Written out rather than pulled in as a dependency so the project keeps
// building with nothing but Qt. Ten versions hold 213 bytes, which is far more
// than a payment payload needs.
//
// The output is verified against a reference implementation in the test suite:
// a QR that does not scan is worse than no QR at all.
class QrCode
{
public:
    // Returns an empty code when the text does not fit in version 10.
    static QrCode encode(const QByteArray &data);

    bool isValid() const { return size > 0; }
    int moduleCount() const { return size; }
    bool moduleAt(int x, int y) const;

    // Renders to a square image, scaled up and with the mandatory quiet zone.
    QImage toImage(int pixelsPerModule = 6, int quietZone = 4) const;

private:
    int size = 0;
    QVector<bool> modules;      // size * size, row major
    QVector<bool> reserved;     // function patterns that data must not overwrite

    void setModule(int x, int y, bool dark);
    bool reservedAt(int x, int y) const;

    void drawFunctionPatterns(int version);
    void drawFinder(int x, int y);
    void drawAlignment(int cx, int cy);
    void drawFormatBits(int mask);
    void drawVersionBits(int version);
    void drawCodewords(const QVector<quint8> &codewords);
    void applyMask(int mask);
    int penalty() const;
};

#endif // QRCODE_H
