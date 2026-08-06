#include "database/DatabaseManager.h"

#include <QCoreApplication>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>
#include <QDebug>

DatabaseManager *DatabaseManager::s_instance = nullptr;

DatabaseManager *DatabaseManager::instance()
{
    if (!s_instance)
        s_instance = new DatabaseManager();
    return s_instance;
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
        "  returnDate TEXT,"
        "  status INTEGER NOT NULL DEFAULT 0,"
        "  FOREIGN KEY(readerID) REFERENCES Readers(readerID),"
        "  FOREIGN KEY(bookID) REFERENCES Books(bookID))");

    if (!ok)
        qWarning() << "createTables failed:" << query.lastError().text();
    return ok;
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
    query.prepare("INSERT INTO Readers (readerID, fullName, phone, email, borrowedCount) "
                  "VALUES (?, ?, ?, ?, ?)");
    query.addBindValue(reader.getReaderID());
    query.addBindValue(reader.getFullName());
    query.addBindValue(reader.getPhone());
    query.addBindValue(reader.getEmail());
    query.addBindValue(reader.getBorrowedCount());
    return query.exec();
}

bool DatabaseManager::updateReader(const Reader &reader)
{
    QSqlQuery query(db);
    query.prepare("UPDATE Readers SET fullName = ?, phone = ?, email = ?, borrowedCount = ? "
                  "WHERE readerID = ?");
    query.addBindValue(reader.getFullName());
    query.addBindValue(reader.getPhone());
    query.addBindValue(reader.getEmail());
    query.addBindValue(reader.getBorrowedCount());
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
    if (query.exec("SELECT readerID, fullName, phone, email, borrowedCount "
                   "FROM Readers ORDER BY readerID")) {
        while (query.next()) {
            readers.append(Reader(query.value(0).toString(), query.value(1).toString(),
                                  query.value(2).toString(), query.value(3).toString(),
                                  query.value(4).toInt()));
        }
    }
    return readers;
}

Reader DatabaseManager::findReaderByID(const QString &readerID)
{
    QSqlQuery query(db);
    query.prepare("SELECT readerID, fullName, phone, email, borrowedCount "
                  "FROM Readers WHERE readerID = ?");
    query.addBindValue(readerID);
    if (query.exec() && query.next()) {
        return Reader(query.value(0).toString(), query.value(1).toString(),
                      query.value(2).toString(), query.value(3).toString(),
                      query.value(4).toInt());
    }
    return Reader();
}

// ------------------------------------------------------- Borrow records

bool DatabaseManager::insertBorrowRecord(const BorrowRecord &record)
{
    QSqlQuery query(db);
    query.prepare("INSERT INTO BorrowRecords (recordID, readerID, bookID, borrowDate, returnDate, status) "
                  "VALUES (?, ?, ?, ?, ?, ?)");
    query.addBindValue(record.getRecordID());
    query.addBindValue(record.getReaderID());
    query.addBindValue(record.getBookID());
    query.addBindValue(record.getBorrowDate());
    query.addBindValue(record.getReturnDate());
    query.addBindValue(record.getStatus() ? 1 : 0);
    return query.exec();
}

bool DatabaseManager::updateBorrowRecord(const BorrowRecord &record)
{
    QSqlQuery query(db);
    query.prepare("UPDATE BorrowRecords SET readerID = ?, bookID = ?, borrowDate = ?, "
                  "returnDate = ?, status = ? WHERE recordID = ?");
    query.addBindValue(record.getReaderID());
    query.addBindValue(record.getBookID());
    query.addBindValue(record.getBorrowDate());
    query.addBindValue(record.getReturnDate());
    query.addBindValue(record.getStatus() ? 1 : 0);
    query.addBindValue(record.getRecordID());
    return query.exec() && query.numRowsAffected() > 0;
}

QList<BorrowRecord> DatabaseManager::getAllBorrowRecords()
{
    QList<BorrowRecord> records;
    QSqlQuery query(db);
    if (query.exec("SELECT recordID, readerID, bookID, borrowDate, returnDate, status "
                   "FROM BorrowRecords ORDER BY recordID")) {
        while (query.next()) {
            records.append(BorrowRecord(query.value(0).toString(), query.value(1).toString(),
                                        query.value(2).toString(), query.value(3).toString(),
                                        query.value(4).toString(), query.value(5).toInt() != 0));
        }
    }
    return records;
}

BorrowRecord DatabaseManager::findBorrowRecordByID(const QString &recordID)
{
    QSqlQuery query(db);
    query.prepare("SELECT recordID, readerID, bookID, borrowDate, returnDate, status "
                  "FROM BorrowRecords WHERE recordID = ?");
    query.addBindValue(recordID);
    if (query.exec() && query.next()) {
        return BorrowRecord(query.value(0).toString(), query.value(1).toString(),
                            query.value(2).toString(), query.value(3).toString(),
                            query.value(4).toString(), query.value(5).toInt() != 0);
    }
    return BorrowRecord();
}
