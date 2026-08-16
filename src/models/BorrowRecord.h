#ifndef BORROWRECORD_H
#define BORROWRECORD_H

#include <QDate>

#include "models/Entity.h"

// One borrow transaction: which reader took which book, when, and when it is
// due back.
// status == false : the book is still out ("Borrowing")
// status == true  : the book has come back ("Returned")
class BorrowRecord : public Entity
{
public:
    BorrowRecord();
    BorrowRecord(const QString &recordID, const QString &readerID, const QString &bookID,
                 const QString &borrowDate, const QString &dueDate,
                 const QString &returnDate, bool status,
                 int finePaid = 0, int renewCount = 0);

    QString getRecordID() const;
    QString getReaderID() const;
    QString getBookID() const;
    QString getBorrowDate() const;
    QString getDueDate() const;
    QString getReturnDate() const;
    bool getStatus() const;
    int getFinePaid() const;    // money already settled or waived
    int getRenewCount() const;

    void setFinePaid(int amount);
    void setRenewCount(int count);
    void setDueDate(const QString &date);
    void setReturnDate(const QString &date);
    void setStatus(bool status);

    // Business helpers
    void markReturned(const QString &returnDate); // close the record
    bool isOverdue(const QDate &today) const;     // still out and past the due date
    int daysLate(const QDate &today) const;       // 0 when not overdue

    // Days past the deadline, measured to the return date once the book is
    // back and to 'today' while it is still out. 0 when it was on time, so it
    // works for both open and closed records.
    int lateDays(const QDate &today) const;

    // Late fee = lateDays * perDay. Charged on returned books too, which is
    // why it cannot simply reuse daysLate().
    int fine(int perDay, const QDate &today) const;

    // What is still owed: the fee minus whatever the librarian has already
    // collected or waived. This is the number that must be able to reach zero,
    // otherwise an old late return would be held against a reader forever.
    int outstandingFine(int perDay, const QDate &today) const;
    bool isFineSettled(int perDay, const QDate &today) const;

    // Still out and so far past the deadline that it counts as lost rather
    // than merely late.
    bool isLost(const QDate &today, int lostAfterDays) const;

    // Entity interface (runtime polymorphism)
    QString getID() const override;
    QString displayInfo() const override;

private:
    QString recordID;
    QString readerID;
    QString bookID;
    QString borrowDate;
    QString dueDate;
    QString returnDate;
    bool status;
    int finePaid;
    int renewCount;
};

#endif // BORROWRECORD_H
