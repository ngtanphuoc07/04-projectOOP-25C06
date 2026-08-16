#ifndef LIBRARYSYSTEM_H
#define LIBRARYSYSTEM_H

#include <QObject>
#include <QSet>

#include "database/DatabaseManager.h"
#include "managers/AccountManager.h"
#include "managers/BookManager.h"
#include "managers/ReaderManager.h"
#include "models/Reservation.h"

// Every dashboard number in one struct, so they can be produced by a single
// pass over the data instead of one full scan per number.
struct LibraryStats
{
    int totalCopies = 0;
    int readers = 0;
    int borrowed = 0;
    int available = 0;
    int overdue = 0;
    int notReturned = 0;
    int finesDue = 0;
};

// The business parameters that used to be compiled in. Stored in the Settings
// table so a librarian can change them without a rebuild.
struct LibraryConfig
{
    int maxBooksPerReader = 5;
    int loanPeriodDays = 14;
    int finePerDay = 5000;      // VND
    int lostAfterDays = 30;
    int maxRenewals = 2;
    int renewalDays = 7;
};

// DESIGN PATTERN 2: FACADE.
// LibrarySystem is the single entry point the GUI uses for every workflow that
// touches more than one object: borrow, return, renew, reserve, register,
// delete-with-account, fines and statistics.
//
// DESIGN PATTERN 3: OBSERVER (via Qt signals/slots).
// Whenever data changes, dataChanged() is emitted and every page that
// subscribed refreshes itself automatically.
class LibrarySystem : public QObject
{
    Q_OBJECT

public:
    explicit LibrarySystem(QObject *parent = nullptr);

    // ---------------------------------------------------------- borrowing
    // overrideOverdue lets a librarian lend anyway to a reader who has an
    // overdue book; a patron borrowing for themselves never gets that option.
    bool borrowBook(const QString &readerID, const QString &bookID,
                    const QString &borrowDate, QString *errorOut = nullptr,
                    bool overrideOverdue = false);
    // fineOut receives the late fee charged for this return (0 when on time).
    bool returnBook(const QString &recordID, const QString &returnDate,
                    QString *errorOut = nullptr, int *fineOut = nullptr);
    bool renewLoan(const QString &recordID, QString *errorOut = nullptr);
    // Undo a loan entered by mistake: the copy goes straight back on the shelf
    // and the record disappears. Only allowed while the book is still out.
    bool cancelLoan(const QString &recordID, QString *errorOut = nullptr);

    // ------------------------------------------------------------- fines
    bool settleFine(const QString &recordID, int amount, QString *errorOut = nullptr);
    bool waiveFine(const QString &recordID, QString *errorOut = nullptr);
    int outstandingFine(const QString &readerID);

    // ------------------------------------------------------ reservations
    bool reserveBook(const QString &readerID, const QString &bookID, QString *errorOut = nullptr);
    bool cancelReservation(const QString &reservationID, QString *errorOut = nullptr);
    // The librarian hands the book over at the desk: this turns an open request
    // into a real loan. Readers cannot create loans themselves.
    bool fulfilReservation(const QString &reservationID, QString *errorOut = nullptr);
    QList<Reservation> getAllReservations();
    QList<Reservation> getOpenReservationsByReader(const QString &readerID);
    QList<Reservation> getOpenReservationsForBook(const QString &bookID);

    // ---------------------------------------------------------- queries
    QList<BorrowRecord> getAllBorrowRecords();
    QList<BorrowRecord> getRecordsByBook(const QString &bookID);
    // Loans of this title that are still open — who has the copies now.
    QList<BorrowRecord> getActiveRecordsByBook(const QString &bookID);
    QList<BorrowRecord> getRecordsByReader(const QString &readerID);
    QList<BorrowRecord> getActiveRecordsByReader(const QString &readerID);
    QList<BorrowRecord> getOverdueRecords();
    QList<BorrowRecord> getLostRecords();
    QList<BorrowRecord> getOverdueRecordsByReader(const QString &readerID);

    // Every reader currently holding an overdue book, found in one pass.
    // Lets a list flag them without running a query per row.
    QSet<QString> readersWithOverdue();

    // ------------------------------------------------------ registration
    bool registerReaderAccount(const QString &username, const QString &password,
                               const QString &fullName, const QString &phone,
                               const QString &email, QString *errorOut = nullptr);
    // Removes the reader and any login bound to them, in one transaction.
    bool deleteReaderAndAccount(const QString &readerID, QString *errorOut = nullptr);

    // ------------------------------------------------------- statistics
    LibraryStats stats();          // one pass, all numbers
    int countTotalBooks();
    int countTotalReaders();
    int countBorrowedBooks();
    int countAvailableBooks();
    int countOverdueBooks();
    int countLostBooks();
    int countTotalFines();

    // ---------------------------------------------------------- plumbing
    BookManager *bookManager();
    ReaderManager *readerManager();
    AccountManager *accountManager();

    LibraryConfig config() const;
    void setConfig(const LibraryConfig &config);   // persists and notifies
    int maxBooksPerReader() const;
    int loanPeriodDays() const;
    int finePerDay() const;
    int lostAfterDays() const;
    static QString formatMoney(int amount);

    // The library's own payment QR, uploaded by a librarian. Empty until one is
    // chosen, in which case PaymentDialog falls back to a generated code.
    // Stored in the database (base64) rather than as a file path, so a moved or
    // deleted file cannot leave the app pointing at nothing.
    QByteArray paymentQrImage();
    bool setPaymentQrImage(const QByteArray &png);

    // Who is signed in, so the audit trail can name them.
    void setCurrentUser(const QString &username);
    QString currentUser() const;
    void audit(const QString &action, const QString &details);

    void notifyDataChanged();
    void seedSampleDataIfEmpty();

signals:
    void dataChanged();

private:
    QString generateRecordID();
    QString generateReservationID();
    void loadConfig();

    DatabaseManager *db;
    BookManager bookMgr;
    ReaderManager readerMgr;
    AccountManager accountMgr;
    LibraryConfig cfg;
    QString user;
};

#endif // LIBRARYSYSTEM_H
