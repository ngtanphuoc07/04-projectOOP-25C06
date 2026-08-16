#ifndef AVATARUTIL_H
#define AVATARUTIL_H

#include <QByteArray>
#include <QPixmap>
#include <QString>

class QWidget;

// Profile pictures: choosing, shrinking, and drawing them.
//
// Pictures are stored as PNG bytes in the database. Anything the user picks is
// cropped square and scaled down first, so a 6 MB phone photo does not become a
// 6 MB row.
namespace AvatarUtil {

// Longest edge of a stored picture, in pixels.
int storedSize();

// Opens a file dialog, then crops and scales the chosen image.
// Returns empty bytes if the user cancelled or the file was not an image.
QByteArray pickImage(QWidget *parent, QString *errorOut = nullptr);

// Decodes stored bytes into a round picture of the requested size.
// Falls back to initials on a coloured circle when there is no picture, so a
// reader without a photo still gets something recognisable.
QPixmap toPixmap(const QByteArray &png, int size, const QString &fallbackText);

} // namespace AvatarUtil

#endif // AVATARUTIL_H
