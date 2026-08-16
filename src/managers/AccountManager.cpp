#include "managers/AccountManager.h"
#include "i18n/Lang.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QMessageAuthenticationCode>
#include <QRandomGenerator>
#include <QRegularExpression>

namespace {
constexpr int kIterations = 120000;   // ~0.1s per attempt on a normal laptop
constexpr int kSaltBytes = 16;
constexpr int kMaxAttempts = 5;
constexpr int kLockoutSeconds = 300;  // 5 minutes
}

AccountManager::AccountManager(DatabaseManager *db)
    : db(db)
{
}

int AccountManager::maxAttempts() { return kMaxAttempts; }
int AccountManager::lockoutSeconds() { return kLockoutSeconds; }

// ---------------------------------------------------------------- hashing

static QByteArray pbkdf2(const QByteArray &password, const QByteArray &salt, int iterations)
{
    // PBKDF2-HMAC-SHA256, one output block (32 bytes) is plenty for a digest.
    QByteArray block = salt;
    block.append(char(0)).append(char(0)).append(char(0)).append(char(1)); // INT(1)

    QByteArray u = QMessageAuthenticationCode::hash(block, password,
                                                    QCryptographicHash::Sha256);
    QByteArray result = u;
    for (int i = 1; i < iterations; ++i) {
        u = QMessageAuthenticationCode::hash(u, password, QCryptographicHash::Sha256);
        for (int b = 0; b < result.size(); ++b)
            result[b] = result[b] ^ u[b];
    }
    return result;
}

QString AccountManager::hashPassword(const QString &password)
{
    QByteArray salt(kSaltBytes, Qt::Uninitialized);
    QRandomGenerator::system()->generate(salt.begin(), salt.end());

    const QByteArray digest = pbkdf2(password.toUtf8(), salt, kIterations);
    return QString("pbkdf2$%1$%2$%3")
        .arg(kIterations)
        .arg(QString::fromLatin1(salt.toHex()),
             QString::fromLatin1(digest.toHex()));
}

bool AccountManager::isLegacyHash(const QString &stored)
{
    // The first version stored a bare 64-character SHA-256 hex digest.
    return !stored.startsWith("pbkdf2$") && stored.length() == 64;
}

QString AccountManager::legacyHash(const QString &username, const QString &password)
{
    const QByteArray salted = (username.toLower() + QStringLiteral(":") + password).toUtf8();
    return QString::fromLatin1(
        QCryptographicHash::hash(salted, QCryptographicHash::Sha256).toHex());
}

bool AccountManager::verifyPassword(const QString &stored, const QString &username,
                                    const QString &password)
{
    if (isLegacyHash(stored))
        return stored == legacyHash(username, password);

    const QStringList parts = stored.split('$');
    if (parts.size() != 4 || parts.at(0) != "pbkdf2")
        return false;

    const int iterations = parts.at(1).toInt();
    const QByteArray salt = QByteArray::fromHex(parts.at(2).toLatin1());
    const QByteArray expected = QByteArray::fromHex(parts.at(3).toLatin1());
    if (iterations <= 0 || salt.isEmpty() || expected.isEmpty())
        return false;

    const QByteArray actual = pbkdf2(password.toUtf8(), salt, iterations);
    // Length-constant comparison: never leak how much of the digest matched.
    if (actual.size() != expected.size())
        return false;
    quint8 diff = 0;
    for (int i = 0; i < actual.size(); ++i)
        diff |= quint8(actual[i] ^ expected[i]);
    return diff == 0;
}

// -------------------------------------------------------------- validation

bool AccountManager::isUsernameValid(const QString &username)
{
    static const QRegularExpression pattern("^[A-Za-z0-9._]{3,20}$");
    return pattern.match(username).hasMatch();
}

bool AccountManager::isPasswordAcceptable(const QString &password, QString *errorOut)
{
    if (password.length() < 6) {
        if (errorOut)
            *errorOut = TR("Password must be at least 6 characters long.");
        return false;
    }
    bool hasLetter = false, hasDigit = false;
    for (const QChar &c : password) {
        if (c.isLetter())
            hasLetter = true;
        else if (c.isDigit())
            hasDigit = true;
    }
    if (!hasLetter || !hasDigit) {
        if (errorOut)
            *errorOut = TR("Password must contain at least one letter and one digit.");
        return false;
    }
    return true;
}

// ------------------------------------------------------------------ login

Account AccountManager::login(const QString &username, const QString &password, QString *errorOut)
{
    const QString name = username.trimmed();
    if (name.isEmpty() || password.isEmpty()) {
        if (errorOut)
            *errorOut = TR("Please enter both a username and a password.");
        return Account();
    }

    Account account = db->findAccountByUsername(name);
    const QDateTime now = QDateTime::currentDateTime();

    // A librarian-disabled account and a temporary lockout share one column but
    // need different messages: "try again in 4,207,680 minutes" helps nobody.
    if (account.isValid() && account.isDisabled()) {
        if (errorOut)
            *errorOut = TR("This account has been locked by a librarian.");
        return Account();
    }
    if (account.isValid() && account.isLockedAt(now)) {
        if (errorOut)
            *errorOut = QString(TR("Too many failed attempts. Try again in %1 minute(s)."))
                            .arg((account.secondsLockedAt(now) + 59) / 60);
        return Account();
    }

    const bool ok = account.isValid()
                    && verifyPassword(account.getPasswordHash(), name, password);

    if (!ok) {
        if (account.isValid()) {
            // Count the failure and lock the account once there are too many,
            // so a password cannot be found by trying every combination.
            account.setFailedAttempts(account.getFailedAttempts() + 1);
            if (account.getFailedAttempts() >= kMaxAttempts) {
                account.setLockedUntil(now.addSecs(kLockoutSeconds).toString(Qt::ISODate));
                account.setFailedAttempts(0);
                db->updateAccount(account);
                db->appendAudit(name, "LOGIN_LOCKED", "too many failed attempts");
                if (errorOut)
                    *errorOut = QString(TR("Too many failed attempts. Try again in %1 minute(s)."))
                                    .arg(kLockoutSeconds / 60);
                return Account();
            }
            db->updateAccount(account);
        }
        // Deliberately the same message for "no such user" and "wrong password":
        // it gives an attacker no way to discover which usernames exist.
        if (errorOut)
            *errorOut = TR("Incorrect username or password.");
        return Account();
    }

    // Signed in: clear the failure counter, and quietly upgrade an old digest
    // to PBKDF2 now that the plain password is available.
    bool dirty = false;
    if (account.getFailedAttempts() != 0 || !account.getLockedUntil().isEmpty()) {
        account.setFailedAttempts(0);
        account.setLockedUntil(QString());
        dirty = true;
    }
    if (isLegacyHash(account.getPasswordHash())) {
        account.setPasswordHash(hashPassword(password));
        dirty = true;
    }
    if (dirty)
        db->updateAccount(account);

    db->appendAudit(name, "LOGIN", account.roleName());
    return account;
}

// --------------------------------------------------------------- accounts

bool AccountManager::addAccount(const Account &account, QString *errorOut)
{
    if (!isUsernameValid(account.getUsername())) {
        if (errorOut)
            *errorOut = TR("Username must be 3–20 characters: letters, digits, dot or underscore.");
        return false;
    }
    if (db->findAccountByUsername(account.getUsername()).isValid()) {
        if (errorOut)
            *errorOut = QString(TR("The username \"%1\" is already taken.")).arg(account.getUsername());
        return false;
    }
    if (!db->insertAccount(account)) {
        if (errorOut)
            *errorOut = TR("Database error while creating the account.");
        return false;
    }
    return true;
}

bool AccountManager::changePassword(const QString &username, const QString &oldPassword,
                                    const QString &newPassword, QString *errorOut)
{
    Account account = db->findAccountByUsername(username);
    if (!account.isValid()) {
        if (errorOut)
            *errorOut = QString(TR("Account \"%1\" was not found.")).arg(username);
        return false;
    }
    if (!verifyPassword(account.getPasswordHash(), username, oldPassword)) {
        if (errorOut)
            *errorOut = TR("The current password is not correct.");
        return false;
    }
    if (!isPasswordAcceptable(newPassword, errorOut))
        return false;

    account.setPasswordHash(hashPassword(newPassword));
    if (!db->updateAccount(account)) {
        if (errorOut)
            *errorOut = TR("Database error while saving the new password.");
        return false;
    }
    db->appendAudit(username, "PASSWORD_CHANGED", QString());
    return true;
}

bool AccountManager::resetPassword(const QString &username, const QString &newPassword,
                                   QString *errorOut)
{
    Account account = db->findAccountByUsername(username);
    if (!account.isValid()) {
        if (errorOut)
            *errorOut = QString(TR("Account \"%1\" was not found.")).arg(username);
        return false;
    }
    if (!isPasswordAcceptable(newPassword, errorOut))
        return false;

    account.setPasswordHash(hashPassword(newPassword));
    account.setFailedAttempts(0);
    account.setLockedUntil(QString());
    if (!db->updateAccount(account)) {
        if (errorOut)
            *errorOut = TR("Database error while saving the new password.");
        return false;
    }
    db->appendAudit(username, "PASSWORD_RESET", "by a librarian");
    return true;
}

bool AccountManager::lockAccount(const QString &username, const QString &requestedBy,
                                 QString *errorOut)
{
    Account account = db->findAccountByUsername(username);
    if (!account.isValid()) {
        if (errorOut)
            *errorOut = QString(TR("Account \"%1\" was not found.")).arg(username);
        return false;
    }
    if (username == requestedBy) {
        if (errorOut)
            *errorOut = TR("You cannot lock the account you are signed in with.");
        return false;
    }
    if (account.isDisabled()) {
        if (errorOut)
            *errorOut = TR("That account is already locked.");
        return false;
    }
    // Locking every librarian out would leave nobody able to unlock anything.
    if (account.isLibrarian()) {
        int usable = 0;
        const QList<Account> all = db->getAllAccounts();
        for (const Account &a : all) {
            if (a.isLibrarian() && !a.isDisabled())
                ++usable;
        }
        if (usable <= 1) {
            if (errorOut)
                *errorOut = TR("This is the only usable librarian account and cannot be locked.");
            return false;
        }
    }

    account.setLockedUntil(Account::disabledSentinel());
    account.setFailedAttempts(0);
    if (!db->updateAccount(account)) {
        if (errorOut)
            *errorOut = TR("Database error while locking the account.");
        return false;
    }
    db->appendAudit(username, "LOCKED", QString("by %1").arg(requestedBy));
    return true;
}

bool AccountManager::unlockAccount(const QString &username, QString *errorOut)
{
    Account account = db->findAccountByUsername(username);
    if (!account.isValid()) {
        if (errorOut)
            *errorOut = QString(TR("Account \"%1\" was not found.")).arg(username);
        return false;
    }
    account.setFailedAttempts(0);
    account.setLockedUntil(QString());
    if (!db->updateAccount(account)) {
        if (errorOut)
            *errorOut = TR("Database error while unlocking the account.");
        return false;
    }
    db->appendAudit(username, "UNLOCKED", QString());
    return true;
}

bool AccountManager::deleteAccount(const QString &username, QString *errorOut)
{
    const Account account = db->findAccountByUsername(username);
    if (!account.isValid()) {
        if (errorOut)
            *errorOut = QString(TR("Account \"%1\" was not found.")).arg(username);
        return false;
    }
    // Removing the last librarian would lock everybody out of the system.
    if (account.isLibrarian()) {
        int librarians = 0;
        const QList<Account> all = db->getAllAccounts();
        for (const Account &a : all) {
            if (a.isLibrarian())
                ++librarians;
        }
        if (librarians <= 1) {
            if (errorOut)
                *errorOut = TR("This is the only librarian account and cannot be removed.");
            return false;
        }
    }
    if (!db->deleteAccount(username)) {
        if (errorOut)
            *errorOut = TR("Database error while deleting the account.");
        return false;
    }
    db->appendAudit(username, "ACCOUNT_DELETED", QString());
    return true;
}

QList<Account> AccountManager::getAllAccounts()
{
    return db->getAllAccounts();
}

Account AccountManager::findAccountByUsername(const QString &username)
{
    return db->findAccountByUsername(username);
}

void AccountManager::seedLibrarianIfEmpty()
{
    if (!db->getAllAccounts().isEmpty())
        return;
    db->insertAccount(Account("admin", hashPassword("123"), Account::Librarian, QString()));
}
