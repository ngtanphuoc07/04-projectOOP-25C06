#ifndef BORROWPAGE_H
#define BORROWPAGE_H

#include <QList>
#include <QWidget>

#include "managers/LibrarySystem.h"
#include "models/BorrowRecord.h"

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
    void onRowSelected();

private:
    void fillTable();

    LibrarySystem *system;
    QList<BorrowRecord> shownRecords;

    QComboBox *readerCombo;
    QComboBox *bookCombo;
    QDateEdit *borrowDateEdit;
    QComboBox *statusFilter;
    QTableWidget *table;
    QDateEdit *returnDateEdit;
    QPushButton *returnButton;
    QLabel *detailLabel;
};

#endif // BORROWPAGE_H
