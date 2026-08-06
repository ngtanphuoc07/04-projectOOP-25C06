#include "managers/BookManager.h"

BookManager::BookManager(DatabaseManager *db)
    : db(db)
{
}

bool BookManager::addBook(const Book &book, QString *errorOut)
{
    if (db->findBookByID(book.getBookID()).isValid()) {
        if (errorOut)
            *errorOut = QString("A book with ID \"%1\" already exists.").arg(book.getBookID());
        return false;
    }
    if (!db->insertBook(book)) {
        if (errorOut)
            *errorOut = "Database error while inserting the book.";
        return false;
    }
    return true;
}

bool BookManager::updateBook(const Book &book, QString *errorOut)
{
    Book existing = db->findBookByID(book.getBookID());
    if (!existing.isValid()) {
        if (errorOut)
            *errorOut = QString("Book \"%1\" was not found.").arg(book.getBookID());
        return false;
    }

    // Copies currently in readers' hands must still fit in the new total.
    int borrowed = existing.getTotalQuantity() - existing.getAvailableQuantity();
    if (book.getTotalQuantity() < borrowed) {
        if (errorOut)
            *errorOut = QString("Total quantity cannot be lower than the %1 copies currently borrowed.")
                            .arg(borrowed);
        return false;
    }

    Book updated = book;
    updated.setAvailableQuantity(book.getTotalQuantity() - borrowed);
    if (!db->updateBook(updated)) {
        if (errorOut)
            *errorOut = "Database error while updating the book.";
        return false;
    }
    return true;
}

bool BookManager::deleteBook(const QString &bookID, QString *errorOut)
{
    Book existing = db->findBookByID(bookID);
    if (!existing.isValid()) {
        if (errorOut)
            *errorOut = QString("Book \"%1\" was not found.").arg(bookID);
        return false;
    }

    // A book that is still in a reader's hands cannot be removed.
    const QList<BorrowRecord> records = db->getAllBorrowRecords();
    for (const BorrowRecord &r : records) {
        if (r.getBookID() == bookID && !r.getStatus()) {
            if (errorOut)
                *errorOut = QString("Book \"%1\" still has copies out on loan (record %2).")
                                .arg(bookID, r.getRecordID());
            return false;
        }
    }

    if (!db->deleteBook(bookID)) {
        if (errorOut)
            *errorOut = "Database error while deleting the book.";
        return false;
    }
    return true;
}

QList<Book> BookManager::getAllBooks()
{
    return db->getAllBooks();
}

QList<Book> BookManager::searchBooksByTitle(const QString &keyword)
{
    QList<Book> result;
    const QList<Book> all = db->getAllBooks();
    for (const Book &b : all) {
        if (b.getTitle().contains(keyword, Qt::CaseInsensitive))
            result.append(b);
    }
    return result;
}

Book BookManager::findBookByID(const QString &bookID)
{
    return db->findBookByID(bookID);
}
