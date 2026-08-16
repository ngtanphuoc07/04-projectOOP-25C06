#ifndef PAYMENTDIALOG_H
#define PAYMENTDIALOG_H

#include <QDialog>

#include "managers/LibrarySystem.h"

class QLabel;
class QLineEdit;

// How a reader settles a late fee: transfer by MoMo, or pay at the counter.
//
// The MoMo tab shows a QR encoding the destination number, the amount and the
// reference. Displaying it does NOT clear the fine — only a librarian pressing
// "Collect fine" does that, once the money has actually arrived.
class PaymentDialog : public QDialog
{
    Q_OBJECT

public:
    PaymentDialog(LibrarySystem *system, const QString &readerID, QWidget *parent = nullptr);

private slots:
    void refreshQr();
    void onSaveQr();

private:
    QWidget *buildMomoTab();
    QWidget *buildCounterTab();
    QString momoPayload() const;

    LibrarySystem *system;
    QString readerID;
    int amount;

    QLineEdit *referenceEdit;
    QLabel *qrLabel;
    QLabel *detailsLabel;
};

#endif // PAYMENTDIALOG_H
