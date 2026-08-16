#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QList>
#include <QSqlDatabase>

#include "models/Account.h"
#include "models/Book.h"
#include "models/Reader.h"
#include "models/BorrowRecord.h"
#include "models/Reservation.h"

// One audit entry: who did what, and when.
struct AuditEntry
{
    QString at;        // ISO timestamp
    QString username;
    QString action;
    QString details;
};

// DESIGN PATTERN 1: SINGLETON.
// Exactly one DatabaseManager exists for the whole application, so every
// module talks to the same SQLite connection. The constructor is private and
// the only way to reach the object is DatabaseManager::instance().
class DatabaseManager
{
public:
    static DatabaseManager *instance();

    DatabaseManager(const DatabaseManager &) = delete;
    DatabaseManager &operator=(const DatabaseManager &) = delete;

    bool connectDatabase();
    bool createTables();
    QString databasePath() const;

    // Multi-step writes wrap themselves in these, so a failure half way
    // through leaves the database exactly as it was.
    bool beginTransaction();
    bool commitTransaction();
    void rollbackTransaction();

    // Books
    bool insertBook(const Book &book);
    bool updateBook(const Book &book);
    bool deleteBook(const QString &bookID);
    QList<Book> getAllBooks();
    Book findBookByID(const QString &bookID);

    // Readers
    bool insertReader(const Reader &reader);
    bool updateReader(const Reader &reader);
    bool deleteReader(const QString &readerID);
    QList<Reader> getAllReaders();
    Reader findReaderByID(const QString &readerID);

    // Borrow records
    bool insertBorrowRecord(const BorrowRecord &record);
    bool updateBorrowRecord(const BorrowRecord &record);
    bool deleteBorrowRecord(const QString &recordID);
    QList<BorrowRecord> getAllBorrowRecords();
    BorrowRecord findBorrowRecordByID(const QString &recordID);

    // Accounts
    bool insertAccount(const Account &account);
    bool updateAccount(const Account &account);
    bool deleteAccount(const QString &username);
    bool deleteAccountsForReader(const QString &readerID);
    QList<Account> getAllAccounts();
    Account findAccountByUsername(const QString &username);

    // Reservations
    bool insertReservation(const Reservation &reservation);
    bool updateReservation(const Reservation &reservation);
    QList<Reservation> getAllReservations();
    Reservation findReservationByID(const QString &reservationID);

    // Settings: business parameters that used to be compiled in.
    QString getSetting(const QString &key, const QString &fallback = QString());
    bool setSetting(const QString &key, const QString &value);

    // Audit trail
    bool appendAudit(const QString &username, const QString &action, const QString &details);
    QList<AuditEntry> getAuditEntries(int limit = 500);

private:
    DatabaseManager() = default;

    // Adds columns and tables introduced after the first release to an
    // existing library.db, so an old database file keeps working.
    void migrateSchema();
    bool columnExists(const QString &table, const QString &column);
    void addColumnIfMissing(const QString &table, const QString &column, const QString &decl);

    QSqlDatabase db;
    static DatabaseManager *s_instance;
};

#endif // DATABASEMANAGER_H
