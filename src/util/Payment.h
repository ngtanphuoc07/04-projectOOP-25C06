#ifndef PAYMENT_H
#define PAYMENT_H

#include <QString>

// Where a fine is paid to, and how the transfer is labelled.
//
// These are rules, not decoration, so they live outside the dialog: the test
// suite checks them without dragging the whole GUI in.
namespace Payment {

// The MoMo number fines are transferred to.
QString momoAccount();

// "<readerID> <four random digits>" — the ID says who paid, the digits keep two
// payments by the same reader apart on the statement.
QString makeReference(const QString &readerID);

// The payload encoded into the QR.
//
// MoMo's personal-transfer QR layout is not published, so this uses the
// widely-cited format. The dialog always shows the number, amount and reference
// as plain text beside the code, so a reader whose app rejects the scan can
// still transfer by hand.
QString momoPayload(const QString &account, int amount, const QString &reference);

} // namespace Payment

#endif // PAYMENT_H
