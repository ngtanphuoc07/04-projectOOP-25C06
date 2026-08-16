#ifndef RESERVATION_H
#define RESERVATION_H

#include "models/Entity.h"

// A reader waiting for a title whose copies are all out.
// When a copy comes back the oldest waiting reservation is the one that gets
// it, so the queue is strictly first come first served.
class Reservation : public Entity
{
public:
    enum State { Waiting = 0, Ready = 1, Fulfilled = 2, Cancelled = 3 };

    Reservation();
    Reservation(const QString &reservationID, const QString &readerID, const QString &bookID,
                const QString &createdDate, State state);

    QString getReservationID() const;
    QString getReaderID() const;
    QString getBookID() const;
    QString getCreatedDate() const;
    State getState() const;

    void setState(State state);

    // Business helpers
    bool isOpen() const;      // still queued or held at the desk
    QString stateName() const;

    // Entity interface (runtime polymorphism)
    QString getID() const override;
    QString displayInfo() const override;

private:
    QString reservationID;
    QString readerID;
    QString bookID;
    QString createdDate;
    State state;
};

#endif // RESERVATION_H
