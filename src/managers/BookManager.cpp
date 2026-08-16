#include "managers/BookManager.h"
#include "i18n/Lang.h"
#include "util/SearchUtil.h"

#include <algorithm>

BookManager::BookManager(DatabaseManager *db)
    : db(db)
{
}

bool BookManager::addBook(const Book &book, QString *errorOut)
{
    if (db->findBookByID(book.getBookID()).isValid()) {
        if (errorOut)
            *errorOut = QString(TR("A book with ID \"%1\" already exists.")).arg(book.getBookID());
        return false;
    }
    if (!db->insertBook(book)) {
        if (errorOut)
            *errorOut = TR("Database error while inserting the book.");
        return false;
    }
    return true;
}

bool BookManager::updateBook(const Book &book, QString *errorOut)
{
    Book existing = db->findBookByID(book.getBookID());
    if (!existing.isValid()) {
        if (errorOut)
            *errorOut = QString(TR("Book \"%1\" was not found.")).arg(book.getBookID());
        return false;
    }

    // Copies currently in readers' hands must still fit in the new total.
    int borrowed = existing.getTotalQuantity() - existing.getAvailableQuantity();
    if (book.getTotalQuantity() < borrowed) {
        if (errorOut)
            *errorOut = QString(TR("Total quantity cannot be lower than the %1 copies currently borrowed."))
                            .arg(borrowed);
        return false;
    }

    Book updated = book;
    updated.setAvailableQuantity(book.getTotalQuantity() - borrowed);
    if (!db->updateBook(updated)) {
        if (errorOut)
            *errorOut = TR("Database error while updating the book.");
        return false;
    }
    return true;
}

bool BookManager::deleteBook(const QString &bookID, QString *errorOut)
{
    Book existing = db->findBookByID(bookID);
    if (!existing.isValid()) {
        if (errorOut)
            *errorOut = QString(TR("Book \"%1\" was not found.")).arg(bookID);
        return false;
    }

    // A book that is still in a reader's hands cannot be removed.
    const QList<BorrowRecord> records = db->getAllBorrowRecords();
    for (const BorrowRecord &r : records) {
        if (r.getBookID() == bookID && !r.getStatus()) {
            if (errorOut)
                *errorOut = QString(TR("Book \"%1\" still has copies out on loan (record %2)."))
                                .arg(bookID, r.getRecordID());
            return false;
        }
    }

    if (!db->deleteBook(bookID)) {
        if (errorOut)
            *errorOut = TR("Database error while deleting the book.");
        return false;
    }
    return true;
}

QList<Book> BookManager::getAllBooks()
{
    return db->getAllBooks();
}

QList<Book> BookManager::searchBooks(const QString &keyword)
{
    // Accent-, case- and order-insensitive, and forgiving of small typos.
    // Results come back best-match first rather than in database order.
    QList<QPair<int, Book>> scored;
    const QList<Book> all = db->getAllBooks();
    for (const Book &b : all) {
        const int s = SearchUtil::scoreAny(
            {b.getTitle(), b.getAuthor(), b.getCategory(), b.getBookID()}, keyword);
        if (s > 0)
            scored.append({s, b});
    }
    std::stable_sort(scored.begin(), scored.end(),
                     [](const QPair<int, Book> &a, const QPair<int, Book> &b) {
                         return a.first > b.first;
                     });

    QList<Book> result;
    result.reserve(scored.size());
    for (const auto &pair : scored)
        result.append(pair.second);
    return result;
}

QString BookManager::nextBookID()
{
    int maxNumber = 0;
    const QList<Book> books = db->getAllBooks();
    for (const Book &b : books) {
        const QString id = b.getBookID();
        if (id.startsWith("B", Qt::CaseInsensitive)) {
            bool ok = false;
            const int n = id.mid(1).toInt(&ok);
            if (ok && n > maxNumber)
                maxNumber = n;
        }
    }
    return QString("B%1").arg(maxNumber + 1, 3, 10, QChar('0'));
}

Book BookManager::findBookByID(const QString &bookID)
{
    return db->findBookByID(bookID);
}
