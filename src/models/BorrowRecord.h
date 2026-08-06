#ifndef BORROWRECORD_H
#define BORROWRECORD_H

#include "models/Entity.h"

// One borrow transaction: which reader took which book and when.
// status == false : the book is still out ("Borrowing")
// status == true  : the book has come back ("Returned")
class BorrowRecord : public Entity
{
public:
    BorrowRecord();
    BorrowRecord(const QString &recordID, const QString &readerID, const QString &bookID,
                 const QString &borrowDate, const QString &returnDate, bool status);

    QString getRecordID() const;
    QString getReaderID() const;
    QString getBookID() const;
    QString getBorrowDate() const;
    QString getReturnDate() const;
    bool getStatus() const;

    void setReturnDate(const QString &date);
    void setStatus(bool status);

    // Business helper: close the record when the book comes back.
    void markReturned(const QString &returnDate);

    // Entity interface (runtime polymorphism)
    QString getID() const override;
    QString displayInfo() const override;

private:
    QString recordID;
    QString readerID;
    QString bookID;
    QString borrowDate;
    QString returnDate;
    bool status;
};

#endif // BORROWRECORD_H
