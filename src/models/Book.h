#ifndef BOOK_H
#define BOOK_H

#include "models/Entity.h"

// Represents one book title in the library (ENCAPSULATION: all state is
// private and only reachable through getters/setters).
class Book : public Entity
{
public:
    Book();
    Book(const QString &bookID, const QString &title, const QString &author,
         const QString &category, int totalQuantity, int availableQuantity);

    QString getBookID() const;
    QString getTitle() const;
    QString getAuthor() const;
    QString getCategory() const;
    int getTotalQuantity() const;
    int getAvailableQuantity() const;

    void setTitle(const QString &title);
    void setAuthor(const QString &author);
    void setCategory(const QString &category);
    void setTotalQuantity(int quantity);
    void setAvailableQuantity(int quantity);

    // Business helpers
    bool isAvailable() const;   // at least one copy left on the shelf
    void borrowBook();          // one copy leaves the library
    void returnBook();          // one copy comes back

    // Entity interface (runtime polymorphism)
    QString getID() const override;
    QString displayInfo() const override;

private:
    QString bookID;
    QString title;
    QString author;
    QString category;
    int totalQuantity;
    int availableQuantity;
};

#endif // BOOK_H
