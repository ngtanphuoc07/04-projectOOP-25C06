#ifndef READER_H
#define READER_H

#include "models/Entity.h"

// Represents a registered library member.
class Reader : public Entity
{
public:
    Reader();
    Reader(const QString &readerID, const QString &fullName, const QString &phone,
           const QString &email, int borrowedCount);

    QString getReaderID() const;
    QString getFullName() const;
    QString getPhone() const;
    QString getEmail() const;
    int getBorrowedCount() const;

    void setFullName(const QString &name);
    void setPhone(const QString &phone);
    void setEmail(const QString &email);
    void setBorrowedCount(int count);

    // Business helpers
    bool canBorrow(int maxBooks) const; // still under the borrow limit?
    void borrowBook();                  // one more book in hand
    void returnBook();                  // gave one book back

    // Entity interface (runtime polymorphism)
    QString getID() const override;
    QString displayInfo() const override;

private:
    QString readerID;
    QString fullName;
    QString phone;
    QString email;
    int borrowedCount;
};

#endif // READER_H
