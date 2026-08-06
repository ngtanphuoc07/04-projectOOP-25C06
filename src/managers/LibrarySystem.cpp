#include "managers/LibrarySystem.h"

#include <QDate>

LibrarySystem::LibrarySystem(QObject *parent)
    : QObject(parent),
      db(DatabaseManager::instance()),
      bookMgr(DatabaseManager::instance()),
      readerMgr(DatabaseManager::instance()),
      maxBorrowBooks(5)
{
}

bool LibrarySystem::borrowBook(const QString &readerID, const QString &bookID,
                               const QString &borrowDate, QString *errorOut)
{
    Reader reader = db->findReaderByID(readerID);
    if (!reader.isValid()) {
        if (errorOut)
            *errorOut = QString("Reader \"%1\" was not found.").arg(readerID);
        return false;
    }

    Book book = db->findBookByID(bookID);
    if (!book.isValid()) {
        if (errorOut)
            *errorOut = QString("Book \"%1\" was not found.").arg(bookID);
        return false;
    }

    if (!reader.canBorrow(maxBorrowBooks)) {
        if (errorOut)
            *errorOut = QString("%1 already has %2 books — the limit is %3.")
                            .arg(reader.getFullName())
                            .arg(reader.getBorrowedCount())
                            .arg(maxBorrowBooks);
        return false;
    }

    if (!book.isAvailable()) {
        if (errorOut)
            *errorOut = QString("No copies of \"%1\" are available right now.").arg(book.getTitle());
        return false;
    }

    // The same reader may not hold two copies of the same title at once.
    const QList<BorrowRecord> records = db->getAllBorrowRecords();
    for (const BorrowRecord &r : records) {
        if (r.getReaderID() == readerID && r.getBookID() == bookID && !r.getStatus()) {
            if (errorOut)
                *errorOut = QString("%1 is already borrowing \"%2\" (record %3).")
                                .arg(reader.getFullName(), book.getTitle(), r.getRecordID());
            return false;
        }
    }

    BorrowRecord record(generateRecordID(), readerID, bookID, borrowDate, QString(), false);
    book.borrowBook();
    reader.borrowBook();

    if (!db->insertBorrowRecord(record) || !db->updateBook(book) || !db->updateReader(reader)) {
        if (errorOut)
            *errorOut = "Database error while saving the borrow transaction.";
        return false;
    }

    emit dataChanged();
    return true;
}

bool LibrarySystem::returnBook(const QString &recordID, const QString &returnDate,
                               QString *errorOut)
{
    BorrowRecord record = db->findBorrowRecordByID(recordID);
    if (!record.isValid()) {
        if (errorOut)
            *errorOut = QString("Borrow record \"%1\" was not found.").arg(recordID);
        return false;
    }
    if (record.getStatus()) {
        if (errorOut)
            *errorOut = QString("Record \"%1\" was already returned on %2.")
                            .arg(recordID, record.getReturnDate());
        return false;
    }

    const QDate borrow = QDate::fromString(record.getBorrowDate(), "yyyy-MM-dd");
    const QDate ret = QDate::fromString(returnDate, "yyyy-MM-dd");
    if (borrow.isValid() && ret.isValid() && ret < borrow) {
        if (errorOut)
            *errorOut = "The return date cannot be earlier than the borrow date.";
        return false;
    }

    record.markReturned(returnDate);

    Book book = db->findBookByID(record.getBookID());
    Reader reader = db->findReaderByID(record.getReaderID());
    if (book.isValid())
        book.returnBook();
    if (reader.isValid())
        reader.returnBook();

    bool ok = db->updateBorrowRecord(record);
    if (book.isValid())
        ok = db->updateBook(book) && ok;
    if (reader.isValid())
        ok = db->updateReader(reader) && ok;

    if (!ok) {
        if (errorOut)
            *errorOut = "Database error while saving the return transaction.";
        return false;
    }

    emit dataChanged();
    return true;
}

QList<BorrowRecord> LibrarySystem::getAllBorrowRecords()
{
    return db->getAllBorrowRecords();
}

int LibrarySystem::countTotalBooks()
{
    int total = 0;
    const QList<Book> books = db->getAllBooks();
    for (const Book &b : books)
        total += b.getTotalQuantity();
    return total;
}

int LibrarySystem::countTotalReaders()
{
    return db->getAllReaders().size();
}

int LibrarySystem::countBorrowedBooks()
{
    int borrowed = 0;
    const QList<Book> books = db->getAllBooks();
    for (const Book &b : books)
        borrowed += b.getTotalQuantity() - b.getAvailableQuantity();
    return borrowed;
}

int LibrarySystem::countAvailableBooks()
{
    int available = 0;
    const QList<Book> books = db->getAllBooks();
    for (const Book &b : books)
        available += b.getAvailableQuantity();
    return available;
}

BookManager *LibrarySystem::bookManager()
{
    return &bookMgr;
}

ReaderManager *LibrarySystem::readerManager()
{
    return &readerMgr;
}

int LibrarySystem::maxBooksPerReader() const
{
    return maxBorrowBooks;
}

void LibrarySystem::notifyDataChanged()
{
    emit dataChanged();
}

void LibrarySystem::seedSampleDataIfEmpty()
{
    if (!db->getAllBooks().isEmpty() || !db->getAllReaders().isEmpty())
        return;

    db->insertBook(Book("B001", "The C++ Programming Language", "Bjarne Stroustrup", "Programming", 5, 5));
    db->insertBook(Book("B002", "Clean Code", "Robert C. Martin", "Software Engineering", 3, 3));
    db->insertBook(Book("B003", "Design Patterns", "Gamma, Helm, Johnson, Vlissides", "Software Engineering", 2, 2));
    db->insertBook(Book("B004", "Introduction to Algorithms", "Cormen, Leiserson, Rivest, Stein", "Algorithms", 4, 4));
    db->insertBook(Book("B005", "Effective Modern C++", "Scott Meyers", "Programming", 3, 3));

    db->insertReader(Reader("R001", "Nguyen Van An", "0901234567", "an.nguyen@example.com", 0));
    db->insertReader(Reader("R002", "Tran Thi Binh", "0912345678", "binh.tran@example.com", 0));
    db->insertReader(Reader("R003", "Le Minh Chau", "0923456789", "chau.le@example.com", 0));
}

QString LibrarySystem::generateRecordID()
{
    // Sequential IDs: BR001, BR002, ...
    int maxNumber = 0;
    const QList<BorrowRecord> records = db->getAllBorrowRecords();
    for (const BorrowRecord &r : records) {
        const QString id = r.getRecordID();
        if (id.startsWith("BR")) {
            bool ok = false;
            const int n = id.mid(2).toInt(&ok);
            if (ok && n > maxNumber)
                maxNumber = n;
        }
    }
    return QString("BR%1").arg(maxNumber + 1, 3, 10, QChar('0'));
}
