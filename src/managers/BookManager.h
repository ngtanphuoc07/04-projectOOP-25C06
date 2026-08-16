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
    // Matches the keyword against ID, title, author and category.
    QList<Book> searchBooks(const QString &keyword);
    Book findBookByID(const QString &bookID);

    // Next free sequential ID (B001, B002, ...) so the user never has to
    // invent one or guess which numbers are taken.
    QString nextBookID();

private:
    DatabaseManager *db;
};

#endif // BOOKMANAGER_H
