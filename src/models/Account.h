#ifndef ACCOUNT_H
#define ACCOUNT_H

#include <QByteArray>
#include <QDateTime>

#include "models/Entity.h"

// A login account. Two roles exist:
//   Librarian - full access to every screen (the staff account).
//   Member    - a reader who registered themselves; sees only the catalogue
//               and their own loans. linkedReaderID points at their Reader row.
//
// The password is never stored: only a salted SHA-256 hash of it (see
// AccountManager::hashPassword).
class Account : public Entity
{
public:
    enum Role { Librarian = 0, Member = 1 };

    Account();
    Account(const QString &username, const QString &passwordHash, Role role,
            const QString &linkedReaderID,
            int failedAttempts = 0, const QString &lockedUntil = QString());

    QString getUsername() const;
    QString getPasswordHash() const;
    Role getRole() const;
    QString getLinkedReaderID() const;

    int getFailedAttempts() const;
    QString getLockedUntil() const;
    QByteArray getAvatar() const;

    void setPasswordHash(const QString &hash);
    void setLinkedReaderID(const QString &readerID);
    void setFailedAttempts(int count);
    void setLockedUntil(const QString &timestamp);
    void setAvatar(const QByteArray &png);

    // Business helpers
    bool isLibrarian() const;
    QString roleName() const;

    // True while the account is serving a lockout after too many bad guesses.
    bool isLockedAt(const QDateTime &now) const;
    int secondsLockedAt(const QDateTime &now) const;

    // A librarian can disable an account outright. That is stored as a lockout
    // with a sentinel far-future date, so one column covers both cases while
    // still telling them apart.
    static QString disabledSentinel();
    bool isDisabled() const;

    // Entity interface (runtime polymorphism)
    QString getID() const override;
    QString displayInfo() const override;

private:
    QString username;
    QString passwordHash;
    Role role;
    QString linkedReaderID;
    int failedAttempts;
    QString lockedUntil;   // ISO timestamp, empty when not locked
    QByteArray avatar;     // used by accounts with no linked reader
};

#endif // ACCOUNT_H
