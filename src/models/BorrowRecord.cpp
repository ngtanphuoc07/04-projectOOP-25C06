#include "models/BorrowRecord.h"

BorrowRecord::BorrowRecord()
    : status(false)
{
}

BorrowRecord::BorrowRecord(const QString &recordID, const QString &readerID, const QString &bookID,
                           const QString &borrowDate, const QString &returnDate, bool status)
    : recordID(recordID), readerID(readerID), bookID(bookID),
      borrowDate(borrowDate), returnDate(returnDate), status(status)
{
}

QString BorrowRecord::getRecordID() const { return recordID; }
QString BorrowRecord::getReaderID() const { return readerID; }
QString BorrowRecord::getBookID() const { return bookID; }
QString BorrowRecord::getBorrowDate() const { return borrowDate; }
QString BorrowRecord::getReturnDate() const { return returnDate; }
bool BorrowRecord::getStatus() const { return status; }

void BorrowRecord::setReturnDate(const QString &date) { returnDate = date; }
void BorrowRecord::setStatus(bool s) { status = s; }

void BorrowRecord::markReturned(const QString &date)
{
    returnDate = date;
    status = true;
}

QString BorrowRecord::getID() const
{
    return recordID;
}

QString BorrowRecord::displayInfo() const
{
    return QString("Record %1 — Reader %2 borrowed Book %3\nBorrowed on: %4    Returned on: %5\nStatus: %6")
        .arg(recordID, readerID, bookID, borrowDate,
             returnDate.isEmpty() ? QStringLiteral("—") : returnDate,
             status ? QStringLiteral("Returned") : QStringLiteral("Borrowing"));
}
