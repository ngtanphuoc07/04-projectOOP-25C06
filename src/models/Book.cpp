#include "models/Book.h"

Book::Book()
    : totalQuantity(0), availableQuantity(0)
{
}

Book::Book(const QString &bookID, const QString &title, const QString &author,
           const QString &category, int totalQuantity, int availableQuantity)
    : bookID(bookID), title(title), author(author), category(category),
      totalQuantity(totalQuantity), availableQuantity(availableQuantity)
{
}

QString Book::getBookID() const { return bookID; }
QString Book::getTitle() const { return title; }
QString Book::getAuthor() const { return author; }
QString Book::getCategory() const { return category; }
int Book::getTotalQuantity() const { return totalQuantity; }
int Book::getAvailableQuantity() const { return availableQuantity; }

void Book::setTitle(const QString &t) { title = t; }
void Book::setAuthor(const QString &a) { author = a; }
void Book::setCategory(const QString &c) { category = c; }
void Book::setTotalQuantity(int quantity) { totalQuantity = quantity; }
void Book::setAvailableQuantity(int quantity) { availableQuantity = quantity; }

bool Book::isAvailable() const
{
    return availableQuantity > 0;
}

void Book::borrowBook()
{
    if (availableQuantity > 0)
        --availableQuantity;
}

void Book::returnBook()
{
    if (availableQuantity < totalQuantity)
        ++availableQuantity;
}

QString Book::getID() const
{
    return bookID;
}

QString Book::displayInfo() const
{
    return QString("Book %1 — \"%2\"\nAuthor: %3    Category: %4\nCopies: %5 available / %6 total")
        .arg(bookID, title, author, category)
        .arg(availableQuantity)
        .arg(totalQuantity);
}
