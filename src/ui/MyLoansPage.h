#ifndef MYLOANSPAGE_H
#define MYLOANSPAGE_H

#include <QList>
#include <QWidget>

#include "managers/LibrarySystem.h"
#include "models/BorrowRecord.h"
#include "models/Reservation.h"

class QLabel;
class QPushButton;
class QTableWidget;

// The screen a signed-in reader sees: the books they are holding right now and
// their own borrowing history. Read-only — a reader can look, not change.
class MyLoansPage : public QWidget
{
    Q_OBJECT

public:
    MyLoansPage(LibrarySystem *system, const QString &readerID, QWidget *parent = nullptr);

private slots:
    void refresh();
    void onCancelReservation();
    void onPrintSlip();
    void onPay();

private:
    QWidget *makeStatCard(const QString &title, QLabel *&numberLabel);
    void fillCurrent(const QList<BorrowRecord> &records);
    void fillHistory(const QList<BorrowRecord> &records);
    void fillReservations();

    LibrarySystem *system;
    QString readerID;

    QLabel *holdingLabel;
    QLabel *limitLabel;
    QLabel *overdueLabel;
    QLabel *fineLabel;
    QLabel *greetingLabel;
    QLabel *statusLabel;
    QTableWidget *currentTable;
    QTableWidget *historyTable;
    QTableWidget *reservationTable;
    QPushButton *cancelReservationButton;
    QPushButton *slipButton;
    QPushButton *payButton;
    QList<Reservation> shownReservations;
};

#endif // MYLOANSPAGE_H
