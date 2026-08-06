#ifndef BOOKMANAGER_H
#define BOOKMANAGER_H

#include <QList>

#include "database/DatabaseManager.h"
#include "models/Book.h"

// Business rules for books. Sits between the GUI and DatabaseManager.
class BookManager
{
public:
    explicit BookManager(DatabaseManager *db);

    bool addBook(const Book &book, QString *errorOut = nullptr);
    bool updateBook(const Book &book, QString *errorOut = nullptr);
    bool deleteBook(const QString &bookID, QString *errorOut = nullptr);

    QList<Book> getAllBooks();
    QList<Book> searchBooksByTitle(const QString &keyword);
    Book findBookByID(const QString &bookID);

private:
    DatabaseManager *db;
};

#endif // BOOKMANAGER_H
