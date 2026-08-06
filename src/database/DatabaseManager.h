#ifndef DATABASEMANAGER_H
#define DATABASEMANAGER_H

#include <QList>
#include <QSqlDatabase>

#include "models/Book.h"
#include "models/Reader.h"
#include "models/BorrowRecord.h"

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
    QList<BorrowRecord> getAllBorrowRecords();
    BorrowRecord findBorrowRecordByID(const QString &recordID);

private:
    DatabaseManager() = default;

    QSqlDatabase db;
    static DatabaseManager *s_instance;
};

#endif // DATABASEMANAGER_H
