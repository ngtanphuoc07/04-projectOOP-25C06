#include "models/Account.h"
#include "i18n/Lang.h"

Account::Account()
    : role(Member), failedAttempts(0)
{
}

Account::Account(const QString &username, const QString &passwordHash, Role role,
                 const QString &linkedReaderID, int failedAttempts,
                 const QString &lockedUntil)
    : username(username), passwordHash(passwordHash), role(role),
      linkedReaderID(linkedReaderID), failedAttempts(failedAttempts),
      lockedUntil(lockedUntil)
{
}

QString Account::getUsername() const { return username; }
QString Account::getPasswordHash() const { return passwordHash; }
Account::Role Account::getRole() const { return role; }
QString Account::getLinkedReaderID() const { return linkedReaderID; }

int Account::getFailedAttempts() const { return failedAttempts; }
QString Account::getLockedUntil() const { return lockedUntil; }
QByteArray Account::getAvatar() const { return avatar; }

void Account::setPasswordHash(const QString &hash) { passwordHash = hash; }
void Account::setLinkedReaderID(const QString &readerID) { linkedReaderID = readerID; }
void Account::setFailedAttempts(int count) { failedAttempts = count; }
void Account::setLockedUntil(const QString &timestamp) { lockedUntil = timestamp; }
void Account::setAvatar(const QByteArray &png) { avatar = png; }

bool Account::isLibrarian() const
{
    return role == Librarian;
}

QString Account::disabledSentinel()
{
    return QStringLiteral("9999-12-31T23:59:59");
}

bool Account::isDisabled() const
{
    return lockedUntil == disabledSentinel();
}

bool Account::isLockedAt(const QDateTime &now) const
{
    return secondsLockedAt(now) > 0;
}

int Account::secondsLockedAt(const QDateTime &now) const
{
    if (lockedUntil.isEmpty())
        return 0;
    const QDateTime until = QDateTime::fromString(lockedUntil, Qt::ISODate);
    if (!until.isValid() || until <= now)
        return 0;
    return static_cast<int>(now.secsTo(until));
}

QString Account::roleName() const
{
    return role == Librarian ? TR("Librarian") : TR("Reader");
}

QString Account::getID() const
{
    return username;
}

QString Account::displayInfo() const
{
    return QString(TR("Account %1\nRole: %2\nLinked reader: %3"))
        .arg(username, roleName(),
             linkedReaderID.isEmpty() ? QStringLiteral("—") : linkedReaderID);
}
