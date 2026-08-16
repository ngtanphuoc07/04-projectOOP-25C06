#ifndef ACCOUNTMANAGER_H
#define ACCOUNTMANAGER_H

#include <QList>

#include "database/DatabaseManager.h"
#include "models/Account.h"

// Business rules for login accounts: credential checking, username rules,
// password hashing and lockout after repeated failures.
class AccountManager
{
public:
    explicit AccountManager(DatabaseManager *db);

    // Returns the matching account on success; an invalid Account otherwise.
    // Counts failures and locks the account for a while once there are too
    // many, so a password cannot be found by brute force.
    Account login(const QString &username, const QString &password, QString *errorOut = nullptr);

    bool addAccount(const Account &account, QString *errorOut = nullptr);
    bool changePassword(const QString &username, const QString &oldPassword,
                        const QString &newPassword, QString *errorOut = nullptr);
    // Librarian-only: set a new password without knowing the old one, which is
    // the only way back in for a reader who forgot theirs.
    bool resetPassword(const QString &username, const QString &newPassword,
                       QString *errorOut = nullptr);
    // Disables an account until a librarian turns it back on. Refuses to
    // disable the account you are signed in with, or the last librarian.
    bool lockAccount(const QString &username, const QString &requestedBy,
                     QString *errorOut = nullptr);
    bool unlockAccount(const QString &username, QString *errorOut = nullptr);
    bool deleteAccount(const QString &username, QString *errorOut = nullptr);

    QList<Account> getAllAccounts();
    Account findAccountByUsername(const QString &username);

    // Creates the built-in librarian account (admin / 123) the first time the
    // application runs against an empty Accounts table.
    void seedLibrarianIfEmpty();

    // PBKDF2-HMAC-SHA256 with a random per-account salt, stored as
    //   pbkdf2$<iterations>$<saltHex>$<hashHex>
    // Deliberately slow, unlike a bare SHA-256, so guessing is expensive.
    static QString hashPassword(const QString &password);
    static bool verifyPassword(const QString &stored, const QString &username,
                               const QString &password);

    // Username: 3-20 characters, letters, digits, dot or underscore.
    static bool isUsernameValid(const QString &username);
    static bool isPasswordAcceptable(const QString &password, QString *errorOut = nullptr);

    static int maxAttempts();
    static int lockoutSeconds();

private:
    // Recognises the original unsalted SHA-256 digests so accounts created
    // before this change can still sign in — and get upgraded when they do.
    static bool isLegacyHash(const QString &stored);
    static QString legacyHash(const QString &username, const QString &password);

    DatabaseManager *db;
};

#endif // ACCOUNTMANAGER_H
