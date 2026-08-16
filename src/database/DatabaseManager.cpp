#include "database/DatabaseManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>

DatabaseManager *DatabaseManager::s_instance = nullptr;

DatabaseManager *DatabaseManager::instance()
{
    // Meyers singleton: the C++11 standard guarantees this initialisation is
    // thread-safe and the object is destroyed at exit, which the old
    // "if (!ptr) new" version provided neither of.
    static DatabaseManager inst;
    s_instance = &inst;
    return s_instance;
}

QString DatabaseManager::databasePath() const
{
    return db.databaseName();
}

bool DatabaseManager::beginTransaction()
{
    return db.transaction();
}

bool DatabaseManager::commitTransaction()
{
    return db.commit();
}

void DatabaseManager::rollbackTransaction()
{
    if (!db.rollback())
        qWarning() << "rollback failed:" << db.lastError().text();
}

bool DatabaseManager::connectDatabase()
{
    db = QSqlDatabase::addDatabase("QSQLITE");
    db.setDatabaseName(QCoreApplication::applicationDirPath() + "/library.db");
    if (!db.open()) {
        qWarning() << "Cannot open database:" << db.lastError().text();
        return false;
    }
    return true;
}

bool DatabaseManager::createTables()
{
    QSqlQuery query(db);

    bool ok = query.exec(
        "CREATE TABLE IF NOT EXISTS Books ("
        "  bookID TEXT PRIMARY KEY,"
        "  title TEXT NOT NULL,"
        "  author TEXT NOT NULL,"
        "  category TEXT,"
        "  totalQuantity INTEGER NOT NULL,"
        "  availableQuantity INTEGER NOT NULL)");

    ok = ok && query.exec(
        "CREATE TABLE IF NOT EXISTS Readers ("
        "  readerID TEXT PRIMARY KEY,"
        "  fullName TEXT NOT NULL,"
        "  phone TEXT,"
        "  email TEXT,"
        "  borrowedCount INTEGER NOT NULL DEFAULT 0)");

    ok = ok && query.exec(
        "CREATE TABLE IF NOT EXISTS BorrowRecords ("
        "  recordID TEXT PRIMARY KEY,"
        "  readerID TEXT NOT NULL,"
        "  bookID TEXT NOT NULL,"
        "  borrowDate TEXT NOT NULL,"
        "  dueDate TEXT,"
        "  returnDate TEXT,"
        "  status INTEGER NOT NULL DEFAULT 0,"
        "  FOREIGN KEY(readerID) REFERENCES Readers(readerID),"
        "  FOREIGN KEY(bookID) REFERENCES Books(bookID))");

    ok = ok && query.exec(
        "CREATE TABLE IF NOT EXISTS Accounts ("
        "  username TEXT PRIMARY KEY,"
        "  passwordHash TEXT NOT NULL,"
        "  role INTEGER NOT NULL DEFAULT 1,"
        "  linkedReaderID TEXT,"
        "  failedAttempts INTEGER NOT NULL DEFAULT 0,"
        "  lockedUntil TEXT,"
        "  FOREIGN KEY(linkedReaderID) REFERENCES Readers(readerID))");

    ok = ok && query.exec(
        "CREATE TABLE IF NOT EXISTS Reservations ("
        "  reservationID TEXT PRIMARY KEY,"
        "  readerID TEXT NOT NULL,"
        "  bookID TEXT NOT NULL,"
        "  createdDate TEXT NOT NULL,"
        "  state INTEGER NOT NULL DEFAULT 0,"
        "  FOREIGN KEY(readerID) REFERENCES Readers(readerID),"
        "  FOREIGN KEY(bookID) REFERENCES Books(bookID))");

    ok = ok && query.exec(
        "CREATE TABLE IF NOT EXISTS Settings ("
        "  key TEXT PRIMARY KEY,"
        "  value TEXT NOT NULL)");

    ok = ok && query.exec(
        "CREATE TABLE IF NOT EXISTS AuditLog ("
        "  logID INTEGER PRIMARY KEY AUTOINCREMENT,"
        "  at TEXT NOT NULL,"
        "  username TEXT,"
        "  action TEXT NOT NULL,"
        "  details TEXT)");

    // Indexes on the columns every history query filters by: without them each
    // lookup is a full table scan.
    query.exec("CREATE INDEX IF NOT EXISTS idxRecordsReader ON BorrowRecords(readerID)");
    query.exec("CREATE INDEX IF NOT EXISTS idxRecordsBook ON BorrowRecords(bookID)");
    query.exec("CREATE INDEX IF NOT EXISTS idxRecordsStatus ON BorrowRecords(status)");

    if (!ok)
        qWarning() << "createTables failed:" << query.lastError().text();

    migrateSchema();
    return ok;
}

// ---------------------------------------------------------- schema upgrade

bool DatabaseManager::columnExists(const QString &table, const QString &column)
{
    QSqlQuery query(db);
    // PRAGMA does not accept bound parameters, but 'table' is always a literal
    // written in this file — never user input.
    if (!query.exec(QString("PRAGMA table_info(%1)").arg(table)))
        return false;
    while (query.next()) {
        if (query.value(1).toString().compare(column, Qt::CaseInsensitive) == 0)
            return true;
    }
    return false;
}

void DatabaseManager::addColumnIfMissing(const QString &table, const QString &column,
                                         const QString &decl)
{
    if (columnExists(table, column))
        return;
    QSqlQuery query(db);
    // Table and column names are literals from this file, never user input.
    if (!query.exec(QString("ALTER TABLE %1 ADD COLUMN %2 %3").arg(table, column, decl)))
        qWarning() << "migrate" << table << column << "failed:" << query.lastError().text();
}

void DatabaseManager::migrateSchema()
{
    QSqlQuery query(db);

    // library.db files created before due dates existed have no dueDate column.
    // CREATE TABLE IF NOT EXISTS leaves them untouched, so add it here and
    // backfill a deadline for the records that are still open.
    const bool hadDueDate = columnExists("BorrowRecords", "dueDate");
    addColumnIfMissing("BorrowRecords", "dueDate", "TEXT");
    if (!hadDueDate) {
        query.exec("UPDATE BorrowRecords SET dueDate = date(borrowDate, '+14 day') "
                   "WHERE dueDate IS NULL OR dueDate = ''");
    }

    // Fines can now be collected or waived, and loans can be renewed.
    addColumnIfMissing("BorrowRecords", "finePaid", "INTEGER NOT NULL DEFAULT 0");
    addColumnIfMissing("BorrowRecords", "renewCount", "INTEGER NOT NULL DEFAULT 0");

    // Login throttling.
    addColumnIfMissing("Accounts", "failedAttempts", "INTEGER NOT NULL DEFAULT 0");
    addColumnIfMissing("Accounts", "lockedUntil", "TEXT");

    // Profile pictures, stored as PNG bytes so one file is still the
    // whole backup.
    addColumnIfMissing("Readers", "avatar", "BLOB");
    addColumnIfMissing("Accounts", "avatar", "BLOB");
}

// ---------------------------------------------------------------- Books

bool DatabaseManager::insertBook(const Book &book)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO Books (bookID, title, author, category, totalQuantity, availableQuantity) "
                  "VALUES (?, ?, ?, ?, ?, ?)");
    query.addBindValue(book.getBookID());
    query.addBindValue(book.getTitle());
    query.addBindValue(book.getAuthor());
    query.addBindValue(book.getCategory());
    query.addBindValue(book.getTotalQuantity());
    query.addBindValue(book.getAvailableQuantity());
    return query.exec();
}

bool DatabaseManager::updateBook(const Book &book)
{
    QSqlQuery query(db);
    query.prepare("UPDATE Books SET title = ?, author = ?, category = ?, "
                  "totalQuantity = ?, availableQuantity = ? WHERE bookID = ?");
    query.addBindValue(book.getTitle());
    query.addBindValue(book.getAuthor());
    query.addBindValue(book.getCategory());
    query.addBindValue(book.getTotalQuantity());
    query.addBindValue(book.getAvailableQuantity());
    query.addBindValue(book.getBookID());
    return query.exec() && query.numRowsAffected() > 0;
}

bool DatabaseManager::deleteBook(const QString &bookID)
{
    QSqlQuery query(db);
    query.prepare("DELETE FROM Books WHERE bookID = ?");
    query.addBindValue(bookID);
    return query.exec() && query.numRowsAffected() > 0;
}

QList<Book> DatabaseManager::getAllBooks()
{
    QList<Book> books;
    QSqlQuery query(db);
    if (query.exec("SELECT bookID, title, author, category, totalQuantity, availableQuantity "
                   "FROM Books ORDER BY bookID")) {
        while (query.next()) {
            books.append(Book(query.value(0).toString(), query.value(1).toString(),
                              query.value(2).toString(), query.value(3).toString(),
                              query.value(4).toInt(), query.value(5).toInt()));
        }
    }
    return books;
}

Book DatabaseManager::findBookByID(const QString &bookID)
{
    QSqlQuery query(db);
    query.prepare("SELECT bookID, title, author, category, totalQuantity, availableQuantity "
                  "FROM Books WHERE bookID = ?");
    query.addBindValue(bookID);
    if (query.exec() && query.next()) {
        return Book(query.value(0).toString(), query.value(1).toString(),
                    query.value(2).toString(), query.value(3).toString(),
                    query.value(4).toInt(), query.value(5).toInt());
    }
    return Book(); // invalid (empty ID) when not found
}

// -------------------------------------------------------------- Readers

bool DatabaseManager::insertReader(const Reader &reader)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO Readers (readerID, fullName, phone, email, borrowedCount, avatar) "
                  "VALUES (?, ?, ?, ?, ?, ?)");
    query.addBindValue(reader.getReaderID());
    query.addBindValue(reader.getFullName());
    query.addBindValue(reader.getPhone());
    query.addBindValue(reader.getEmail());
    query.addBindValue(reader.getBorrowedCount());
    query.addBindValue(reader.getAvatar());
    return query.exec();
}

bool DatabaseManager::updateReader(const Reader &reader)
{
    QSqlQuery query(db);
    query.prepare("UPDATE Readers SET fullName = ?, phone = ?, email = ?, borrowedCount = ?, "
                  "avatar = ? WHERE readerID = ?");
    query.addBindValue(reader.getFullName());
    query.addBindValue(reader.getPhone());
    query.addBindValue(reader.getEmail());
    query.addBindValue(reader.getBorrowedCount());
    query.addBindValue(reader.getAvatar());
    query.addBindValue(reader.getReaderID());
    return query.exec() && query.numRowsAffected() > 0;
}

bool DatabaseManager::deleteReader(const QString &readerID)
{
    QSqlQuery query(db);
    query.prepare("DELETE FROM Readers WHERE readerID = ?");
    query.addBindValue(readerID);
    return query.exec() && query.numRowsAffected() > 0;
}

QList<Reader> DatabaseManager::getAllReaders()
{
    QList<Reader> readers;
    QSqlQuery query(db);
    if (query.exec("SELECT readerID, fullName, phone, email, borrowedCount, avatar "
                   "FROM Readers ORDER BY readerID")) {
        while (query.next()) {
            Reader r(query.value(0).toString(), query.value(1).toString(),
                     query.value(2).toString(), query.value(3).toString(),
                     query.value(4).toInt());
            r.setAvatar(query.value(5).toByteArray());
            readers.append(r);
        }
    }
    return readers;
}

Reader DatabaseManager::findReaderByID(const QString &readerID)
{
    QSqlQuery query(db);
    query.prepare("SELECT readerID, fullName, phone, email, borrowedCount, avatar "
                  "FROM Readers WHERE readerID = ?");
    query.addBindValue(readerID);
    if (query.exec() && query.next()) {
        Reader r(query.value(0).toString(), query.value(1).toString(),
                 query.value(2).toString(), query.value(3).toString(),
                 query.value(4).toInt());
        r.setAvatar(query.value(5).toByteArray());
        return r;
    }
    return Reader();
}

// ------------------------------------------------------- Borrow records

bool DatabaseManager::insertBorrowRecord(const BorrowRecord &record)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO BorrowRecords "
                  "(recordID, readerID, bookID, borrowDate, dueDate, returnDate, status, finePaid, renewCount) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(record.getRecordID());
    query.addBindValue(record.getReaderID());
    query.addBindValue(record.getBookID());
    query.addBindValue(record.getBorrowDate());
    query.addBindValue(record.getDueDate());
    query.addBindValue(record.getReturnDate());
    query.addBindValue(record.getStatus() ? 1 : 0);
    query.addBindValue(record.getFinePaid());
    query.addBindValue(record.getRenewCount());
    return query.exec();
}

bool DatabaseManager::updateBorrowRecord(const BorrowRecord &record)
{
    QSqlQuery query(db);
    query.prepare("UPDATE BorrowRecords SET readerID = ?, bookID = ?, borrowDate = ?, "
                  "dueDate = ?, returnDate = ?, status = ?, finePaid = ?, renewCount = ? "
                  "WHERE recordID = ?");
    query.addBindValue(record.getReaderID());
    query.addBindValue(record.getBookID());
    query.addBindValue(record.getBorrowDate());
    query.addBindValue(record.getDueDate());
    query.addBindValue(record.getReturnDate());
    query.addBindValue(record.getStatus() ? 1 : 0);
    query.addBindValue(record.getFinePaid());
    query.addBindValue(record.getRenewCount());
    query.addBindValue(record.getRecordID());
    return query.exec() && query.numRowsAffected() > 0;
}

QList<BorrowRecord> DatabaseManager::getAllBorrowRecords()
{
    QList<BorrowRecord> records;
    QSqlQuery query(db);
    if (query.exec("SELECT recordID, readerID, bookID, borrowDate, dueDate, returnDate, status, "
                   "finePaid, renewCount "
                   "FROM BorrowRecords ORDER BY recordID")) {
        while (query.next()) {
            records.append(BorrowRecord(query.value(0).toString(), query.value(1).toString(),
                                        query.value(2).toString(), query.value(3).toString(),
                                        query.value(4).toString(), query.value(5).toString(),
                                        query.value(6).toInt() != 0,
                                        query.value(7).toInt(), query.value(8).toInt()));
        }
    }
    return records;
}

BorrowRecord DatabaseManager::findBorrowRecordByID(const QString &recordID)
{
    QSqlQuery query(db);
    query.prepare("SELECT recordID, readerID, bookID, borrowDate, dueDate, returnDate, status, "
                   "finePaid, renewCount "
                  "FROM BorrowRecords WHERE recordID = ?");
    query.addBindValue(recordID);
    if (query.exec() && query.next()) {
        return BorrowRecord(query.value(0).toString(), query.value(1).toString(),
                            query.value(2).toString(), query.value(3).toString(),
                            query.value(4).toString(), query.value(5).toString(),
                            query.value(6).toInt() != 0,
                            query.value(7).toInt(), query.value(8).toInt());
    }
    return BorrowRecord();
}

// ------------------------------------------------------------- Accounts

bool DatabaseManager::insertAccount(const Account &account)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO Accounts (username, passwordHash, role, linkedReaderID, failedAttempts, lockedUntil, avatar) "
                  "VALUES (?, ?, ?, ?, ?, ?, ?)");
    query.addBindValue(account.getUsername());
    query.addBindValue(account.getPasswordHash());
    query.addBindValue(static_cast<int>(account.getRole()));
    query.addBindValue(account.getLinkedReaderID());
    query.addBindValue(account.getFailedAttempts());
    query.addBindValue(account.getLockedUntil());
    query.addBindValue(account.getAvatar());
    return query.exec();
}

bool DatabaseManager::updateAccount(const Account &account)
{
    QSqlQuery query(db);
    query.prepare("UPDATE Accounts SET passwordHash = ?, role = ?, linkedReaderID = ?, "
                  "failedAttempts = ?, lockedUntil = ?, avatar = ? WHERE username = ?");
    query.addBindValue(account.getPasswordHash());
    query.addBindValue(static_cast<int>(account.getRole()));
    query.addBindValue(account.getLinkedReaderID());
    query.addBindValue(account.getFailedAttempts());
    query.addBindValue(account.getLockedUntil());
    query.addBindValue(account.getAvatar());
    query.addBindValue(account.getUsername());
    return query.exec() && query.numRowsAffected() > 0;
}

bool DatabaseManager::deleteAccount(const QString &username)
{
    QSqlQuery query(db);
    query.prepare("DELETE FROM Accounts WHERE username = ?");
    query.addBindValue(username);
    return query.exec() && query.numRowsAffected() > 0;
}

QList<Account> DatabaseManager::getAllAccounts()
{
    QList<Account> accounts;
    QSqlQuery query(db);
    if (query.exec("SELECT username, passwordHash, role, linkedReaderID, failedAttempts, lockedUntil, avatar "
                   "FROM Accounts ORDER BY username")) {
        while (query.next()) {
            Account a(query.value(0).toString(), query.value(1).toString(),
                      static_cast<Account::Role>(query.value(2).toInt()),
                      query.value(3).toString(), query.value(4).toInt(),
                      query.value(5).toString());
            a.setAvatar(query.value(6).toByteArray());
            accounts.append(a);
        }
    }
    return accounts;
}

Account DatabaseManager::findAccountByUsername(const QString &username)
{
    QSqlQuery query(db);
    query.prepare("SELECT username, passwordHash, role, linkedReaderID, failedAttempts, lockedUntil, avatar "
                  "FROM Accounts WHERE username = ?");
    query.addBindValue(username);
    if (query.exec() && query.next()) {
        Account a(query.value(0).toString(), query.value(1).toString(),
                  static_cast<Account::Role>(query.value(2).toInt()),
                  query.value(3).toString(), query.value(4).toInt(),
                  query.value(5).toString());
        a.setAvatar(query.value(6).toByteArray());
        return a;
    }
    return Account(); // invalid (empty username) when not found
}

// -------------------------------------------------------- extra deletions

bool DatabaseManager::deleteBorrowRecord(const QString &recordID)
{
    QSqlQuery query(db);
    query.prepare("DELETE FROM BorrowRecords WHERE recordID = ?");
    query.addBindValue(recordID);
    return query.exec() && query.numRowsAffected() > 0;
}

bool DatabaseManager::deleteAccountsForReader(const QString &readerID)
{
    // Deleting a reader must take their login with it, otherwise the account
    // survives pointing at a row that no longer exists.
    QSqlQuery query(db);
    query.prepare("DELETE FROM Accounts WHERE linkedReaderID = ?");
    query.addBindValue(readerID);
    return query.exec();
}

// --------------------------------------------------------- Reservations

bool DatabaseManager::insertReservation(const Reservation &reservation)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO Reservations (reservationID, readerID, bookID, createdDate, state) "
                  "VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(reservation.getReservationID());
    query.addBindValue(reservation.getReaderID());
    query.addBindValue(reservation.getBookID());
    query.addBindValue(reservation.getCreatedDate());
    query.addBindValue(static_cast<int>(reservation.getState()));
    return query.exec();
}

bool DatabaseManager::updateReservation(const Reservation &reservation)
{
    QSqlQuery query(db);
    query.prepare("UPDATE Reservations SET readerID = ?, bookID = ?, createdDate = ?, state = ? "
                  "WHERE reservationID = ?");
    query.addBindValue(reservation.getReaderID());
    query.addBindValue(reservation.getBookID());
    query.addBindValue(reservation.getCreatedDate());
    query.addBindValue(static_cast<int>(reservation.getState()));
    query.addBindValue(reservation.getReservationID());
    return query.exec() && query.numRowsAffected() > 0;
}

QList<Reservation> DatabaseManager::getAllReservations()
{
    QList<Reservation> list;
    QSqlQuery query(db);
    if (query.exec("SELECT reservationID, readerID, bookID, createdDate, state "
                   "FROM Reservations ORDER BY reservationID")) {
        while (query.next()) {
            list.append(Reservation(query.value(0).toString(), query.value(1).toString(),
                                    query.value(2).toString(), query.value(3).toString(),
                                    static_cast<Reservation::State>(query.value(4).toInt())));
        }
    }
    return list;
}

Reservation DatabaseManager::findReservationByID(const QString &reservationID)
{
    QSqlQuery query(db);
    query.prepare("SELECT reservationID, readerID, bookID, createdDate, state "
                  "FROM Reservations WHERE reservationID = ?");
    query.addBindValue(reservationID);
    if (query.exec() && query.next()) {
        return Reservation(query.value(0).toString(), query.value(1).toString(),
                           query.value(2).toString(), query.value(3).toString(),
                           static_cast<Reservation::State>(query.value(4).toInt()));
    }
    return Reservation();
}

// -------------------------------------------------------------- Settings

QString DatabaseManager::getSetting(const QString &key, const QString &fallback)
{
    QSqlQuery query(db);
    query.prepare("SELECT value FROM Settings WHERE key = ?");
    query.addBindValue(key);
    if (query.exec() && query.next())
        return query.value(0).toString();
    return fallback;
}

bool DatabaseManager::setSetting(const QString &key, const QString &value)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO Settings (key, value) VALUES (?, ?) "
                  "ON CONFLICT(key) DO UPDATE SET value = excluded.value");
    query.addBindValue(key);
    query.addBindValue(value);
    return query.exec();
}

// ------------------------------------------------------------- Audit log

bool DatabaseManager::appendAudit(const QString &username, const QString &action,
                                  const QString &details)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO AuditLog (at, username, action, details) VALUES (?, ?, ?, ?)");
    query.addBindValue(QDateTime::currentDateTime().toString(Qt::ISODate));
    query.addBindValue(username);
    query.addBindValue(action);
    query.addBindValue(details);
    return query.exec();
}

QList<AuditEntry> DatabaseManager::getAuditEntries(int limit)
{
    QList<AuditEntry> list;
    QSqlQuery query(db);
    query.prepare("SELECT at, username, action, details FROM AuditLog "
                  "ORDER BY logID DESC LIMIT ?");
    query.addBindValue(limit);
    if (query.exec()) {
        while (query.next()) {
            AuditEntry e;
            e.at = query.value(0).toString();
            e.username = query.value(1).toString();
            e.action = query.value(2).toString();
            e.details = query.value(3).toString();
            list.append(e);
        }
    }
    return list;
}
