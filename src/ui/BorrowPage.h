#ifndef BORROWPAGE_H
#define BORROWPAGE_H

#include <QList>
#include <QWidget>

#include "managers/LibrarySystem.h"
#include "models/BorrowRecord.h"
#include "models/Reservation.h"

class QComboBox;
class QDateEdit;
class QLabel;
class QPushButton;
class QTableWidget;

// Borrow / return workflow screen: create borrow records, return books,
// browse the full transaction history with a status filter.
class BorrowPage : public QWidget
{
    Q_OBJECT

public:
    explicit BorrowPage(LibrarySystem *system, QWidget *parent = nullptr);

private slots:
    void refresh();
    void onBorrow();
    void onReturn();
    void onRenew();
    void onCancelLoan();
    void onSettleFine();
    void onWaiveFine();
    void onRowSelected();
    void onFulfilRequest();
    void onRejectRequest();

private:
    void fillTable();
    void fillRequests();

    LibrarySystem *system;
    QList<BorrowRecord> shownRecords;

    QComboBox *readerCombo;
    QComboBox *bookCombo;
    QDateEdit *borrowDateEdit;
    QComboBox *statusFilter;
    QTableWidget *table;
    QDateEdit *returnDateEdit;
    QPushButton *returnButton;
    QPushButton *renewButton;
    QPushButton *cancelButton;
    QPushButton *settleButton;
    QPushButton *waiveButton;
    QLabel *detailLabel;
    QString keepSelectedRecordID;   // survives a refresh
    QTableWidget *requestTable;
    QPushButton *fulfilButton;
    QPushButton *rejectButton;
    QList<Reservation> shownRequests;
};

#endif // BORROWPAGE_H
