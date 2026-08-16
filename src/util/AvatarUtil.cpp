#include "util/AvatarUtil.h"

#include <QBuffer>
#include <QFileDialog>
#include <QFileInfo>
#include <QFont>
#include <QImage>
#include <QPainter>
#include <QPainterPath>

#include "i18n/Lang.h"

namespace {
constexpr int kStoredSize = 256;
constexpr qint64 kMaxFileBytes = 12 * 1024 * 1024;   // 12 MB before we refuse
}

namespace AvatarUtil {

int storedSize() { return kStoredSize; }

QByteArray pickImage(QWidget *parent, QString *errorOut)
{
    const QString path = QFileDialog::getOpenFileName(
        parent, TR("Choose a profile picture"), QString(),
        TR("Images (*.png *.jpg *.jpeg *.bmp *.gif *.webp)"));
    if (path.isEmpty())
        return {};                       // cancelled, not an error

    if (QFileInfo(path).size() > kMaxFileBytes) {
        if (errorOut)
            *errorOut = TR("That image is too large. Please choose one under 12 MB.");
        return {};
    }

    QImage image(path);
    if (image.isNull()) {
        if (errorOut)
            *errorOut = TR("That file could not be read as an image.");
        return {};
    }

    // Centre-crop to a square first, so scaling never squashes the face.
    const int side = qMin(image.width(), image.height());
    image = image.copy((image.width() - side) / 2, (image.height() - side) / 2, side, side)
                .scaled(kStoredSize, kStoredSize,
                        Qt::IgnoreAspectRatio, Qt::SmoothTransformation);

    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) {
        if (errorOut)
            *errorOut = TR("That image could not be converted.");
        return {};
    }
    return png;
}

QPixmap toPixmap(const QByteArray &png, int size, const QString &fallbackText)
{
    QPixmap source;
    if (!png.isEmpty())
        source.loadFromData(png, "PNG");

    QPixmap out(size, size);
    out.fill(Qt::transparent);

    QPainter painter(&out);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setRenderHint(QPainter::SmoothPixmapTransform);

    QPainterPath circle;
    circle.addEllipse(0, 0, size, size);
    painter.setClipPath(circle);

    if (!source.isNull()) {
        painter.drawPixmap(0, 0, source.scaled(size, size, Qt::IgnoreAspectRatio,
                                               Qt::SmoothTransformation));
    } else {
        // No picture: initials on a colour derived from the text, so the same
        // person always gets the same colour.
        QString initials;
        const QStringList words = fallbackText.simplified().split(' ', Qt::SkipEmptyParts);
        if (!words.isEmpty())
            initials += words.first().at(0);
        if (words.size() > 1)
            initials += words.last().at(0);
        if (initials.isEmpty())
            initials = "?";

        uint hash = 0;
        for (const QChar &c : fallbackText)
            hash = hash * 31 + c.unicode();
        const QColor bg = QColor::fromHsv(int(hash % 360), 120, 190);

        painter.fillRect(0, 0, size, size, bg);
        QFont font = painter.font();
        font.setPixelSize(qMax(10, size / 2));
        font.setBold(true);
        painter.setFont(font);
        painter.setPen(Qt::white);
        painter.drawText(QRect(0, 0, size, size), Qt::AlignCenter, initials.toUpper());
    }
    painter.end();
    return out;
}

} // namespace AvatarUtil
