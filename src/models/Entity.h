#ifndef ENTITY_H
#define ENTITY_H

#include <QString>

// Abstract base class of every domain object in the system.
// Demonstrates ABSTRACTION (pure virtual interface) and is the root of the
// INHERITANCE hierarchy: Book, Reader and BorrowRecord all derive from Entity.
// Code that holds an Entity& / Entity* uses runtime POLYMORPHISM when calling
// getID() / displayInfo().
class Entity
{
public:
    virtual ~Entity() = default;

    // Unique identifier of the object (bookID / readerID / recordID).
    virtual QString getID() const = 0;

    // Human readable, multi-line description shown in the detail panels.
    virtual QString displayInfo() const = 0;

    // True when the object was actually loaded/created (non-empty ID).
    bool isValid() const { return !getID().isEmpty(); }
};

#endif // ENTITY_H
