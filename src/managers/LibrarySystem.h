#ifndef LIBRARYSYSTEM_H
#define LIBRARYSYSTEM_H

#include <QObject>

#include "database/DatabaseManager.h"
#include "managers/BookManager.h"
#include "managers/ReaderManager.h"

// DESIGN PATTERN 2: FACADE.
// LibrarySystem is the single entry point the GUI uses for the borrow/return
// workflow and the statistics. It coordinates DatabaseManager, BookManager
// and ReaderManager so no window has to know the individual steps
// (create record -> decrease stock -> increase reader count -> persist).
//
// DESIGN PATTERN 3: OBSERVER (via Qt signals/slots).
// Whenever data changes, dataChanged() is emitted and every page that
// subscribed refreshes itself automatically.
class LibrarySystem : public QObject
{
    Q_OBJECT

public:
    explicit LibrarySystem(QObject *parent = nullptr);

    // Borrow / return workflow (business logic beyond CRUD)
    bool borrowBook(const QString &readerID, const QString &bookID,
                    const QString &borrowDate, QString *errorOut = nullptr);
    bool returnBook(const QString &recordID, const QString &returnDate,
                    QString *errorOut = nullptr);
    QList<BorrowRecord> getAllBorrowRecords();

    // Statistics for the dashboard / report feature
    int countTotalBooks();      // sum of all copies owned by the library
    int countTotalReaders();
    int countBorrowedBooks();   // copies currently in readers' hands
    int countAvailableBooks();  // copies currently on the shelves

    // Sub-managers for the CRUD pages
    BookManager *bookManager();
    ReaderManager *readerManager();

    int maxBooksPerReader() const;

    // Pages call this after a successful CRUD operation so that every
    // observer window refreshes (Observer pattern).
    void notifyDataChanged();

    // Fills the database with demo rows the first time the app runs.
    void seedSampleDataIfEmpty();

signals:
    void dataChanged();

private:
    QString generateRecordID();

    DatabaseManager *db;
    BookManager bookMgr;
    ReaderManager readerMgr;
    int maxBorrowBooks;
};

#endif // LIBRARYSYSTEM_H
