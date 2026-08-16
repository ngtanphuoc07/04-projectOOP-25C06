#include "models/BorrowRecord.h"

BorrowRecord::BorrowRecord()
    : status(false), finePaid(0), renewCount(0)
{
}

BorrowRecord::BorrowRecord(const QString &recordID, const QString &readerID, const QString &bookID,
                           const QString &borrowDate, const QString &dueDate,
                           const QString &returnDate, bool status,
                           int finePaid, int renewCount)
    : recordID(recordID), readerID(readerID), bookID(bookID),
      borrowDate(borrowDate), dueDate(dueDate), returnDate(returnDate), status(status),
      finePaid(finePaid), renewCount(renewCount)
{
}

QString BorrowRecord::getRecordID() const { return recordID; }
QString BorrowRecord::getReaderID() const { return readerID; }
QString BorrowRecord::getBookID() const { return bookID; }
QString BorrowRecord::getBorrowDate() const { return borrowDate; }
QString BorrowRecord::getDueDate() const { return dueDate; }
QString BorrowRecord::getReturnDate() const { return returnDate; }
bool BorrowRecord::getStatus() const { return status; }
int BorrowRecord::getFinePaid() const { return finePaid; }
int BorrowRecord::getRenewCount() const { return renewCount; }

void BorrowRecord::setFinePaid(int amount) { finePaid = amount; }
void BorrowRecord::setRenewCount(int count) { renewCount = count; }
void BorrowRecord::setDueDate(const QString &date) { dueDate = date; }
void BorrowRecord::setReturnDate(const QString &date) { returnDate = date; }
void BorrowRecord::setStatus(bool s) { status = s; }

void BorrowRecord::markReturned(const QString &date)
{
    returnDate = date;
    status = true;
}

bool BorrowRecord::isOverdue(const QDate &today) const
{
    if (status)
        return false; // already back on the shelf
    const QDate due = QDate::fromString(dueDate, "yyyy-MM-dd");
    return due.isValid() && due < today;
}

int BorrowRecord::daysLate(const QDate &today) const
{
    if (!isOverdue(today))
        return 0;
    return QDate::fromString(dueDate, "yyyy-MM-dd").daysTo(today);
}

int BorrowRecord::lateDays(const QDate &today) const
{
    const QDate due = QDate::fromString(dueDate, "yyyy-MM-dd");
    if (!due.isValid())
        return 0;

    // Once the book is back the damage is fixed: measure to the return date,
    // not to today, otherwise an old late return would keep growing forever.
    QDate reference = today;
    if (status) {
        reference = QDate::fromString(returnDate, "yyyy-MM-dd");
        if (!reference.isValid())
            return 0;
    }
    const int days = due.daysTo(reference);
    return days > 0 ? days : 0;
}

int BorrowRecord::fine(int perDay, const QDate &today) const
{
    return lateDays(today) * perDay;
}

int BorrowRecord::outstandingFine(int perDay, const QDate &today) const
{
    const int owed = fine(perDay, today) - finePaid;
    return owed > 0 ? owed : 0;
}

bool BorrowRecord::isFineSettled(int perDay, const QDate &today) const
{
    return outstandingFine(perDay, today) == 0;
}

bool BorrowRecord::isLost(const QDate &today, int lostAfterDays) const
{
    return !status && lateDays(today) >= lostAfterDays;
}

QString BorrowRecord::getID() const
{
    return recordID;
}

QString BorrowRecord::displayInfo() const
{
    QString state = status ? QStringLiteral("Returned") : QStringLiteral("Borrowing");
    const int late = daysLate(QDate::currentDate());
    if (late > 0)
        state = QString("OVERDUE by %1 day(s)").arg(late);

    return QString("Record %1 — Reader %2 borrowed Book %3\n"
                   "Borrowed on: %4    Due: %5    Returned on: %6\n"
                   "Status: %7")
        .arg(recordID, readerID, bookID, borrowDate,
             dueDate.isEmpty() ? QStringLiteral("—") : dueDate,
             returnDate.isEmpty() ? QStringLiteral("—") : returnDate,
             state);
}
