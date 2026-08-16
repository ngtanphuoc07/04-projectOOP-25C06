#include "managers/LibrarySystem.h"
#include "i18n/Lang.h"

#include <QDate>
#include <QSet>

LibrarySystem::LibrarySystem(QObject *parent)
    : QObject(parent),
      db(DatabaseManager::instance()),
      bookMgr(DatabaseManager::instance()),
      readerMgr(DatabaseManager::instance()),
      accountMgr(DatabaseManager::instance())
{
    loadConfig();
}

bool LibrarySystem::borrowBook(const QString &readerID, const QString &bookID,
                               const QString &borrowDate, QString *errorOut,
                               bool overrideOverdue)
{
    Reader reader = db->findReaderByID(readerID);
    if (!reader.isValid()) {
        if (errorOut)
            *errorOut = QString(TR("Reader \"%1\" was not found.")).arg(readerID);
        return false;
    }

    Book book = db->findBookByID(bookID);
    if (!book.isValid()) {
        if (errorOut)
            *errorOut = QString(TR("Book \"%1\" was not found.")).arg(bookID);
        return false;
    }

    if (!reader.canBorrow(cfg.maxBooksPerReader)) {
        if (errorOut)
            *errorOut = QString(TR("%1 already has %2 books — the limit is %3."))
                            .arg(reader.getFullName())
                            .arg(reader.getBorrowedCount())
                            .arg(cfg.maxBooksPerReader);
        return false;
    }

    if (!book.isAvailable()) {
        if (errorOut)
            *errorOut = QString(TR("No copies of \"%1\" are available right now.")).arg(book.getTitle());
        return false;
    }

    // Someone sitting on an overdue book may not take out anything else until
    // they have brought it back. Without this the borrow limit is the only
    // brake, and a reader could keep accumulating late books.
    const QList<BorrowRecord> late = overrideOverdue ? QList<BorrowRecord>()
                                                    : getOverdueRecordsByReader(readerID);
    if (!late.isEmpty()) {
        if (errorOut) {
            const BorrowRecord &worst = late.first();
            const Book lateBook = db->findBookByID(worst.getBookID());
            *errorOut = QString(TR("%1 has an overdue book and cannot borrow until it is returned:\n"
                                   "\"%2\" (record %3) was due on %4 — %5 day(s) late, fine so far %6."))
                            .arg(reader.getFullName(),
                                 lateBook.isValid() ? lateBook.getTitle() : worst.getBookID(),
                                 worst.getRecordID(), worst.getDueDate())
                            .arg(worst.lateDays(QDate::currentDate()))
                            .arg(formatMoney(worst.fine(cfg.finePerDay, QDate::currentDate())));
        }
        return false;
    }

    // The same reader may not hold two copies of the same title at once.
    const QList<BorrowRecord> records = db->getAllBorrowRecords();
    for (const BorrowRecord &r : records) {
        if (r.getReaderID() == readerID && r.getBookID() == bookID && !r.getStatus()) {
            if (errorOut)
                *errorOut = QString(TR("%1 is already borrowing \"%2\" (record %3)."))
                                .arg(reader.getFullName(), book.getTitle(), r.getRecordID());
            return false;
        }
    }

    // The deadline is derived from the borrow date, so a back-dated loan gets a
    // back-dated deadline and shows up as overdue straight away.
    const QDate start = QDate::fromString(borrowDate, "yyyy-MM-dd");
    const QString dueDate = start.isValid()
                                ? start.addDays(cfg.loanPeriodDays).toString("yyyy-MM-dd")
                                : QDate::currentDate().addDays(cfg.loanPeriodDays).toString("yyyy-MM-dd");

    BorrowRecord record(generateRecordID(), readerID, bookID, borrowDate, dueDate,
                        QString(), false);
    book.borrowBook();
    reader.borrowBook();

    // All three writes succeed together or none of them do: without this a
    // failure after the first INSERT would leave a loan recorded against stock
    // that was never decremented.
    db->beginTransaction();
    if (!db->insertBorrowRecord(record) || !db->updateBook(book) || !db->updateReader(reader)) {
        db->rollbackTransaction();
        if (errorOut)
            *errorOut = TR("Database error while saving the borrow transaction.");
        return false;
    }
    if (!db->commitTransaction()) {
        db->rollbackTransaction();
        if (errorOut)
            *errorOut = TR("Database error while saving the borrow transaction.");
        return false;
    }

    audit("BORROW", QString("%1 borrowed %2 (record %3, due %4)")
                        .arg(readerID, bookID, record.getRecordID(), dueDate));
    emit dataChanged();
    return true;
}

bool LibrarySystem::returnBook(const QString &recordID, const QString &returnDate,
                               QString *errorOut, int *fineOut)
{
    if (fineOut)
        *fineOut = 0;

    BorrowRecord record = db->findBorrowRecordByID(recordID);
    if (!record.isValid()) {
        if (errorOut)
            *errorOut = QString(TR("Borrow record \"%1\" was not found.")).arg(recordID);
        return false;
    }
    if (record.getStatus()) {
        if (errorOut)
            *errorOut = QString(TR("Record \"%1\" was already returned on %2."))
                            .arg(recordID, record.getReturnDate());
        return false;
    }

    const QDate borrow = QDate::fromString(record.getBorrowDate(), "yyyy-MM-dd");
    const QDate ret = QDate::fromString(returnDate, "yyyy-MM-dd");
    if (borrow.isValid() && ret.isValid() && ret < borrow) {
        if (errorOut)
            *errorOut = TR("The return date cannot be earlier than the borrow date.");
        return false;
    }

    record.markReturned(returnDate);

    // What is still owed after anything already collected or waived.
    if (fineOut)
        *fineOut = record.outstandingFine(cfg.finePerDay, QDate::currentDate());

    Book book = db->findBookByID(record.getBookID());
    Reader reader = db->findReaderByID(record.getReaderID());
    if (book.isValid())
        book.returnBook();
    if (reader.isValid())
        reader.returnBook();

    db->beginTransaction();
    bool ok = db->updateBorrowRecord(record);
    if (book.isValid())
        ok = db->updateBook(book) && ok;
    if (reader.isValid())
        ok = db->updateReader(reader) && ok;

    if (!ok || !db->commitTransaction()) {
        db->rollbackTransaction();
        if (errorOut)
            *errorOut = TR("Database error while saving the return transaction.");
        return false;
    }

    audit("RETURN", QString("record %1 returned on %2").arg(recordID, returnDate));

    // A copy just came back: if anyone was queued for this title, the oldest
    // reservation becomes ready for pickup.
    if (book.isValid()) {
        const QList<Reservation> queue = getOpenReservationsForBook(book.getBookID());
        for (const Reservation &res : queue) {
            if (res.getState() == Reservation::Waiting) {
                Reservation ready = res;
                ready.setState(Reservation::Ready);
                db->updateReservation(ready);
                audit("RESERVATION_READY", QString("%1 is ready for %2")
                                               .arg(book.getBookID(), res.getReaderID()));
                break; // strictly first come, first served
            }
        }
    }

    emit dataChanged();
    return true;
}

QList<BorrowRecord> LibrarySystem::getAllBorrowRecords()
{
    return db->getAllBorrowRecords();
}

// ------------------------------------------------------- history queries

QList<BorrowRecord> LibrarySystem::getRecordsByBook(const QString &bookID)
{
    QList<BorrowRecord> result;
    const QList<BorrowRecord> all = db->getAllBorrowRecords();
    for (const BorrowRecord &r : all) {
        if (r.getBookID() == bookID)
            result.append(r);
    }
    return result;
}

QList<BorrowRecord> LibrarySystem::getActiveRecordsByBook(const QString &bookID)
{
    QList<BorrowRecord> result;
    const QList<BorrowRecord> all = db->getAllBorrowRecords();
    for (const BorrowRecord &r : all) {
        if (r.getBookID() == bookID && !r.getStatus())
            result.append(r);
    }
    return result;
}

QList<BorrowRecord> LibrarySystem::getRecordsByReader(const QString &readerID)
{
    QList<BorrowRecord> result;
    const QList<BorrowRecord> all = db->getAllBorrowRecords();
    for (const BorrowRecord &r : all) {
        if (r.getReaderID() == readerID)
            result.append(r);
    }
    return result;
}

QList<BorrowRecord> LibrarySystem::getActiveRecordsByReader(const QString &readerID)
{
    QList<BorrowRecord> result;
    const QList<BorrowRecord> all = db->getAllBorrowRecords();
    for (const BorrowRecord &r : all) {
        if (r.getReaderID() == readerID && !r.getStatus())
            result.append(r);
    }
    return result;
}

QList<BorrowRecord> LibrarySystem::getOverdueRecords()
{
    QList<BorrowRecord> result;
    const QDate today = QDate::currentDate();
    const QList<BorrowRecord> all = db->getAllBorrowRecords();
    for (const BorrowRecord &r : all) {
        if (r.isOverdue(today))
            result.append(r);
    }
    return result;
}

QList<BorrowRecord> LibrarySystem::getLostRecords()
{
    QList<BorrowRecord> result;
    const QDate today = QDate::currentDate();
    const QList<BorrowRecord> all = db->getAllBorrowRecords();
    for (const BorrowRecord &r : all) {
        if (r.isLost(today, cfg.lostAfterDays))
            result.append(r);
    }
    return result;
}

QList<BorrowRecord> LibrarySystem::getOverdueRecordsByReader(const QString &readerID)
{
    QList<BorrowRecord> result;
    const QDate today = QDate::currentDate();
    const QList<BorrowRecord> all = db->getAllBorrowRecords();
    for (const BorrowRecord &r : all) {
        if (r.getReaderID() == readerID && r.isOverdue(today))
            result.append(r);
    }
    return result;
}

// ---------------------------------------------------- self-registration

bool LibrarySystem::registerReaderAccount(const QString &username, const QString &password,
                                          const QString &fullName, const QString &phone,
                                          const QString &email, QString *errorOut)
{
    if (!AccountManager::isUsernameValid(username)) {
        if (errorOut)
            *errorOut = TR("Username must be 3–20 characters: letters, digits, dot or underscore.");
        return false;
    }
    if (!AccountManager::isPasswordAcceptable(password, errorOut))
        return false;
    if (fullName.trimmed().isEmpty()) {
        if (errorOut)
            *errorOut = TR("Full name must not be empty.");
        return false;
    }
    if (accountMgr.findAccountByUsername(username).isValid()) {
        if (errorOut)
            *errorOut = QString(TR("The username \"%1\" is already taken.")).arg(username);
        return false;
    }

    // Reader row first: if the account insert failed afterwards we would rather
    // leave an unused reader than an account pointing at nothing.
    const QString readerID = readerMgr.nextReaderID();
    Reader reader(readerID, fullName.trimmed(), phone.trimmed(), email.trimmed(), 0);
    if (!readerMgr.addReader(reader, errorOut))
        return false;

    Account account(username, AccountManager::hashPassword(password),
                    Account::Member, readerID);
    if (!accountMgr.addAccount(account, errorOut)) {
        readerMgr.deleteReader(readerID); // roll the half-finished registration back
        return false;
    }

    emit dataChanged();
    return true;
}

BookManager *LibrarySystem::bookManager()
{
    return &bookMgr;
}

ReaderManager *LibrarySystem::readerManager()
{
    return &readerMgr;
}

AccountManager *LibrarySystem::accountManager()
{
    return &accountMgr;
}

QString LibrarySystem::formatMoney(int amount)
{
    // 125000 -> "125,000 ₫" — grouped so large fines stay readable.
    QString digits = QString::number(amount);
    for (int i = digits.length() - 3; i > 0; i -= 3)
        digits.insert(i, ',');
    return digits + QString::fromUtf8(" ₫");
}

void LibrarySystem::notifyDataChanged()
{
    emit dataChanged();
}

void LibrarySystem::seedSampleDataIfEmpty()
{
    // The librarian account is seeded independently of the sample rows, so it
    // still appears if the books and readers were added by hand.
    accountMgr.seedLibrarianIfEmpty();

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

    // One demo reader login so the member view can be shown without having to
    // register first. Password: 123
    db->insertAccount(Account("an", AccountManager::hashPassword("reader123"),
                              Account::Member, "R001"));
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

// ============================================================ new workflows

bool LibrarySystem::renewLoan(const QString &recordID, QString *errorOut)
{
    BorrowRecord record = db->findBorrowRecordByID(recordID);
    if (!record.isValid()) {
        if (errorOut)
            *errorOut = QString(TR("Borrow record \"%1\" was not found.")).arg(recordID);
        return false;
    }
    if (record.getStatus()) {
        if (errorOut)
            *errorOut = TR("This loan is already closed and cannot be renewed.");
        return false;
    }
    if (record.getRenewCount() >= cfg.maxRenewals) {
        if (errorOut)
            *errorOut = QString(TR("This loan has already been renewed %1 time(s); the limit is %2."))
                            .arg(record.getRenewCount()).arg(cfg.maxRenewals);
        return false;
    }
    // Renewing an already-late book would let a reader escape the fine, and
    // renewing one somebody is queued for would keep them waiting forever.
    if (record.isOverdue(QDate::currentDate())) {
        if (errorOut)
            *errorOut = TR("An overdue book cannot be renewed — please return it first.");
        return false;
    }
    if (!getOpenReservationsForBook(record.getBookID()).isEmpty()) {
        if (errorOut)
            *errorOut = TR("Another reader is waiting for this title, so it cannot be renewed.");
        return false;
    }

    const QDate oldDue = QDate::fromString(record.getDueDate(), "yyyy-MM-dd");
    const QDate base = oldDue.isValid() ? oldDue : QDate::currentDate();
    record.setDueDate(base.addDays(cfg.renewalDays).toString("yyyy-MM-dd"));
    record.setRenewCount(record.getRenewCount() + 1);

    if (!db->updateBorrowRecord(record)) {
        if (errorOut)
            *errorOut = TR("Database error while renewing the loan.");
        return false;
    }

    audit("RENEW", QString("record %1 extended to %2").arg(recordID, record.getDueDate()));
    emit dataChanged();
    return true;
}

bool LibrarySystem::cancelLoan(const QString &recordID, QString *errorOut)
{
    BorrowRecord record = db->findBorrowRecordByID(recordID);
    if (!record.isValid()) {
        if (errorOut)
            *errorOut = QString(TR("Borrow record \"%1\" was not found.")).arg(recordID);
        return false;
    }
    if (record.getStatus()) {
        if (errorOut)
            *errorOut = TR("This loan is already closed. Only an open loan entered by mistake can be cancelled.");
        return false;
    }

    Book book = db->findBookByID(record.getBookID());
    Reader reader = db->findReaderByID(record.getReaderID());
    if (book.isValid())
        book.returnBook();
    if (reader.isValid())
        reader.returnBook();

    db->beginTransaction();
    bool ok = db->deleteBorrowRecord(recordID);
    if (book.isValid())
        ok = db->updateBook(book) && ok;
    if (reader.isValid())
        ok = db->updateReader(reader) && ok;
    if (!ok || !db->commitTransaction()) {
        db->rollbackTransaction();
        if (errorOut)
            *errorOut = TR("Database error while cancelling the loan.");
        return false;
    }

    audit("CANCEL_LOAN", QString("record %1 (%2 / %3) removed as a mistake")
                             .arg(recordID, record.getReaderID(), record.getBookID()));
    emit dataChanged();
    return true;
}

// ------------------------------------------------------------------ fines

bool LibrarySystem::settleFine(const QString &recordID, int amount, QString *errorOut)
{
    BorrowRecord record = db->findBorrowRecordByID(recordID);
    if (!record.isValid()) {
        if (errorOut)
            *errorOut = QString(TR("Borrow record \"%1\" was not found.")).arg(recordID);
        return false;
    }
    if (amount <= 0) {
        if (errorOut)
            *errorOut = TR("The amount collected must be greater than zero.");
        return false;
    }
    const int owed = record.outstandingFine(cfg.finePerDay, QDate::currentDate());
    if (owed == 0) {
        if (errorOut)
            *errorOut = TR("There is nothing left to pay on this record.");
        return false;
    }
    if (amount > owed) {
        if (errorOut)
            *errorOut = QString(TR("Only %1 is outstanding on this record."))
                            .arg(formatMoney(owed));
        return false;
    }

    record.setFinePaid(record.getFinePaid() + amount);
    if (!db->updateBorrowRecord(record)) {
        if (errorOut)
            *errorOut = TR("Database error while recording the payment.");
        return false;
    }

    audit("FINE_PAID", QString("record %1: %2 collected").arg(recordID, formatMoney(amount)));
    emit dataChanged();
    return true;
}

bool LibrarySystem::waiveFine(const QString &recordID, QString *errorOut)
{
    BorrowRecord record = db->findBorrowRecordByID(recordID);
    if (!record.isValid()) {
        if (errorOut)
            *errorOut = QString(TR("Borrow record \"%1\" was not found.")).arg(recordID);
        return false;
    }
    const int owed = record.outstandingFine(cfg.finePerDay, QDate::currentDate());
    if (owed == 0) {
        if (errorOut)
            *errorOut = TR("There is nothing left to pay on this record.");
        return false;
    }

    // Waiving is recorded as "paid" so the arithmetic stays in one place; the
    // audit line is what distinguishes forgiveness from money changing hands.
    record.setFinePaid(record.getFinePaid() + owed);
    if (!db->updateBorrowRecord(record)) {
        if (errorOut)
            *errorOut = TR("Database error while waiving the fine.");
        return false;
    }

    audit("FINE_WAIVED", QString("record %1: %2 waived").arg(recordID, formatMoney(owed)));
    emit dataChanged();
    return true;
}

int LibrarySystem::outstandingFine(const QString &readerID)
{
    int total = 0;
    const QDate today = QDate::currentDate();
    const QList<BorrowRecord> all = db->getAllBorrowRecords();
    for (const BorrowRecord &r : all) {
        if (r.getReaderID() == readerID)
            total += r.outstandingFine(cfg.finePerDay, today);
    }
    return total;
}

// ----------------------------------------------------------- reservations

bool LibrarySystem::reserveBook(const QString &readerID, const QString &bookID, QString *errorOut)
{
    const Reader reader = db->findReaderByID(readerID);
    if (!reader.isValid()) {
        if (errorOut)
            *errorOut = QString(TR("Reader \"%1\" was not found.")).arg(readerID);
        return false;
    }
    const Book book = db->findBookByID(bookID);
    if (!book.isValid()) {
        if (errorOut)
            *errorOut = QString(TR("Book \"%1\" was not found.")).arg(bookID);
        return false;
    }
    const QList<Reservation> mine = getOpenReservationsByReader(readerID);
    for (const Reservation &r : mine) {
        if (r.getBookID() == bookID) {
            if (errorOut)
                *errorOut = TR("You are already in the queue for this title.");
            return false;
        }
    }
    // Holding a copy already means there is nothing to wait for.
    const QList<BorrowRecord> active = getActiveRecordsByReader(readerID);
    for (const BorrowRecord &r : active) {
        if (r.getBookID() == bookID) {
            if (errorOut)
                *errorOut = TR("You are already holding this title.");
            return false;
        }
    }

    // A copy on the shelf can be collected straight away; otherwise the reader
    // joins the queue and is promoted when one comes back.
    const Reservation::State state = book.isAvailable() ? Reservation::Ready
                                                        : Reservation::Waiting;
    Reservation reservation(generateReservationID(), readerID, bookID,
                            QDate::currentDate().toString("yyyy-MM-dd"), state);
    if (!db->insertReservation(reservation)) {
        if (errorOut)
            *errorOut = TR("Database error while saving the reservation.");
        return false;
    }

    audit("RESERVE", QString("%1 queued for %2").arg(readerID, bookID));
    emit dataChanged();
    return true;
}

bool LibrarySystem::cancelReservation(const QString &reservationID, QString *errorOut)
{
    Reservation reservation = db->findReservationByID(reservationID);
    if (!reservation.isValid()) {
        if (errorOut)
            *errorOut = TR("That reservation was not found.");
        return false;
    }
    if (!reservation.isOpen()) {
        if (errorOut)
            *errorOut = TR("That reservation is already closed.");
        return false;
    }

    reservation.setState(Reservation::Cancelled);
    if (!db->updateReservation(reservation)) {
        if (errorOut)
            *errorOut = TR("Database error while cancelling the reservation.");
        return false;
    }

    audit("RESERVATION_CANCELLED", reservationID);
    emit dataChanged();
    return true;
}

bool LibrarySystem::fulfilReservation(const QString &reservationID, QString *errorOut)
{
    Reservation reservation = db->findReservationByID(reservationID);
    if (!reservation.isValid()) {
        if (errorOut)
            *errorOut = TR("That reservation was not found.");
        return false;
    }
    if (!reservation.isOpen()) {
        if (errorOut)
            *errorOut = TR("That reservation is already closed.");
        return false;
    }

    // Every borrowing rule still applies — the request is not a way around the
    // limit, the duplicate check or the overdue block.
    if (!borrowBook(reservation.getReaderID(), reservation.getBookID(),
                    QDate::currentDate().toString("yyyy-MM-dd"), errorOut))
        return false;

    reservation.setState(Reservation::Fulfilled);
    if (!db->updateReservation(reservation)) {
        if (errorOut)
            *errorOut = TR("Database error while closing the request.");
        return false;
    }

    audit("REQUEST_FULFILLED", QString("%1 -> %2 / %3")
                                   .arg(reservationID, reservation.getReaderID(),
                                        reservation.getBookID()));
    emit dataChanged();
    return true;
}

QList<Reservation> LibrarySystem::getAllReservations()
{
    return db->getAllReservations();
}

QList<Reservation> LibrarySystem::getOpenReservationsByReader(const QString &readerID)
{
    QList<Reservation> out;
    const QList<Reservation> all = db->getAllReservations();
    for (const Reservation &r : all) {
        if (r.getReaderID() == readerID && r.isOpen())
            out.append(r);
    }
    return out;
}

QList<Reservation> LibrarySystem::getOpenReservationsForBook(const QString &bookID)
{
    QList<Reservation> out;
    const QList<Reservation> all = db->getAllReservations();
    for (const Reservation &r : all) {
        if (r.getBookID() == bookID && r.isOpen())
            out.append(r);
    }
    return out;
}

// -------------------------------------------------------------- deletion

bool LibrarySystem::deleteReaderAndAccount(const QString &readerID, QString *errorOut)
{
    // ReaderManager refuses while books are still out; that check runs first.
    db->beginTransaction();
    if (!readerMgr.deleteReader(readerID, errorOut)) {
        db->rollbackTransaction();
        return false;
    }
    // The login must go with the reader, otherwise it survives pointing at a
    // row that no longer exists and every screen behind it breaks.
    if (!db->deleteAccountsForReader(readerID) || !db->commitTransaction()) {
        db->rollbackTransaction();
        if (errorOut)
            *errorOut = TR("Database error while deleting the reader.");
        return false;
    }

    audit("DELETE_READER", readerID);
    emit dataChanged();
    return true;
}

// ------------------------------------------------------------ statistics

QSet<QString> LibrarySystem::readersWithOverdue()
{
    // One pass over the records instead of one query per reader, which is what
    // the readers list used to do (and what made it quadratic).
    QSet<QString> out;
    const QDate today = QDate::currentDate();
    const QList<BorrowRecord> all = db->getAllBorrowRecords();
    for (const BorrowRecord &r : all) {
        if (r.isOverdue(today))
            out.insert(r.getReaderID());
    }
    return out;
}

LibraryStats LibrarySystem::stats()
{
    LibraryStats s;
    const QDate today = QDate::currentDate();

    const QList<Book> books = db->getAllBooks();
    for (const Book &b : books) {
        s.totalCopies += b.getTotalQuantity();
        s.available += b.getAvailableQuantity();
    }
    s.borrowed = s.totalCopies - s.available;
    s.readers = db->getAllReaders().size();

    const QList<BorrowRecord> records = db->getAllBorrowRecords();
    for (const BorrowRecord &r : records) {
        if (r.isOverdue(today))
            ++s.overdue;
        if (r.isLost(today, cfg.lostAfterDays))
            ++s.notReturned;
        s.finesDue += r.outstandingFine(cfg.finePerDay, today);
    }
    return s;
}

int LibrarySystem::countTotalBooks()     { return stats().totalCopies; }
int LibrarySystem::countTotalReaders()   { return stats().readers; }
int LibrarySystem::countBorrowedBooks()  { return stats().borrowed; }
int LibrarySystem::countAvailableBooks() { return stats().available; }
int LibrarySystem::countOverdueBooks()   { return stats().overdue; }
int LibrarySystem::countLostBooks()      { return stats().notReturned; }
int LibrarySystem::countTotalFines()     { return stats().finesDue; }

// ------------------------------------------------------ config and audit

void LibrarySystem::loadConfig()
{
    cfg.maxBooksPerReader = db->getSetting("maxBooksPerReader", "5").toInt();
    cfg.loanPeriodDays    = db->getSetting("loanPeriodDays", "14").toInt();
    cfg.finePerDay        = db->getSetting("finePerDay", "5000").toInt();
    cfg.lostAfterDays     = db->getSetting("lostAfterDays", "30").toInt();
    cfg.maxRenewals       = db->getSetting("maxRenewals", "2").toInt();
    cfg.renewalDays       = db->getSetting("renewalDays", "7").toInt();
}

LibraryConfig LibrarySystem::config() const
{
    return cfg;
}

void LibrarySystem::setConfig(const LibraryConfig &config)
{
    cfg = config;
    db->setSetting("maxBooksPerReader", QString::number(cfg.maxBooksPerReader));
    db->setSetting("loanPeriodDays", QString::number(cfg.loanPeriodDays));
    db->setSetting("finePerDay", QString::number(cfg.finePerDay));
    db->setSetting("lostAfterDays", QString::number(cfg.lostAfterDays));
    db->setSetting("maxRenewals", QString::number(cfg.maxRenewals));
    db->setSetting("renewalDays", QString::number(cfg.renewalDays));

    audit("SETTINGS", QString("limit=%1 loan=%2d fine=%3 lost=%4d renew=%5x%6d")
                          .arg(cfg.maxBooksPerReader).arg(cfg.loanPeriodDays)
                          .arg(cfg.finePerDay).arg(cfg.lostAfterDays)
                          .arg(cfg.maxRenewals).arg(cfg.renewalDays));
    emit dataChanged();
}

QByteArray LibrarySystem::paymentQrImage()
{
    return QByteArray::fromBase64(db->getSetting("paymentQrImage").toLatin1());
}

bool LibrarySystem::setPaymentQrImage(const QByteArray &png)
{
    if (!db->setSetting("paymentQrImage", QString::fromLatin1(png.toBase64())))
        return false;
    audit("PAYMENT_QR", png.isEmpty() ? QStringLiteral("removed")
                                      : QString("uploaded, %1 bytes").arg(png.size()));
    emit dataChanged();
    return true;
}

int LibrarySystem::maxBooksPerReader() const { return cfg.maxBooksPerReader; }
int LibrarySystem::loanPeriodDays() const    { return cfg.loanPeriodDays; }
int LibrarySystem::finePerDay() const        { return cfg.finePerDay; }
int LibrarySystem::lostAfterDays() const     { return cfg.lostAfterDays; }

void LibrarySystem::setCurrentUser(const QString &username)
{
    user = username;
}

QString LibrarySystem::currentUser() const
{
    return user;
}

void LibrarySystem::audit(const QString &action, const QString &details)
{
    db->appendAudit(user, action, details);
}

QString LibrarySystem::generateReservationID()
{
    int maxNumber = 0;
    const QList<Reservation> all = db->getAllReservations();
    for (const Reservation &r : all) {
        const QString id = r.getReservationID();
        if (id.startsWith("RS")) {
            bool ok = false;
            const int n = id.mid(2).toInt(&ok);
            if (ok && n > maxNumber)
                maxNumber = n;
        }
    }
    return QString("RS%1").arg(maxNumber + 1, 3, 10, QChar('0'));
}
