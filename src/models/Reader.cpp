#include "models/Reader.h"

Reader::Reader()
    : borrowedCount(0)
{
}

Reader::Reader(const QString &readerID, const QString &fullName, const QString &phone,
               const QString &email, int borrowedCount)
    : readerID(readerID), fullName(fullName), phone(phone), email(email),
      borrowedCount(borrowedCount)
{
}

QString Reader::getReaderID() const { return readerID; }
QString Reader::getFullName() const { return fullName; }
QString Reader::getPhone() const { return phone; }
QString Reader::getEmail() const { return email; }
int Reader::getBorrowedCount() const { return borrowedCount; }

void Reader::setFullName(const QString &name) { fullName = name; }
void Reader::setPhone(const QString &p) { phone = p; }
void Reader::setEmail(const QString &e) { email = e; }
void Reader::setBorrowedCount(int count) { borrowedCount = count; }

bool Reader::canBorrow(int maxBooks) const
{
    return borrowedCount < maxBooks;
}

void Reader::borrowBook()
{
    ++borrowedCount;
}

void Reader::returnBook()
{
    if (borrowedCount > 0)
        --borrowedCount;
}

QString Reader::getID() const
{
    return readerID;
}

QString Reader::displayInfo() const
{
    return QString("Reader %1 — %2\nPhone: %3    Email: %4\nBooks currently borrowed: %5")
        .arg(readerID, fullName, phone, email)
        .arg(borrowedCount);
}
