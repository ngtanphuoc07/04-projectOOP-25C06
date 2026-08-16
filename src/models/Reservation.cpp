#include "models/Reservation.h"

#include "i18n/Lang.h"

Reservation::Reservation()
    : state(Waiting)
{
}

Reservation::Reservation(const QString &reservationID, const QString &readerID,
                         const QString &bookID, const QString &createdDate, State state)
    : reservationID(reservationID), readerID(readerID), bookID(bookID),
      createdDate(createdDate), state(state)
{
}

QString Reservation::getReservationID() const { return reservationID; }
QString Reservation::getReaderID() const { return readerID; }
QString Reservation::getBookID() const { return bookID; }
QString Reservation::getCreatedDate() const { return createdDate; }
Reservation::State Reservation::getState() const { return state; }

void Reservation::setState(State s) { state = s; }

bool Reservation::isOpen() const
{
    return state == Waiting || state == Ready;
}

QString Reservation::stateName() const
{
    switch (state) {
    case Waiting:   return TR("Waiting");
    case Ready:     return TR("Ready for pickup");
    case Fulfilled: return TR("Fulfilled");
    case Cancelled: return TR("Cancelled");
    }
    return QString();
}

QString Reservation::getID() const
{
    return reservationID;
}

QString Reservation::displayInfo() const
{
    return QString(TR("Reservation %1 — Reader %2 is waiting for Book %3\n"
                      "Requested on: %4\nStatus: %5"))
        .arg(reservationID, readerID, bookID, createdDate, stateName());
}
