#include "util/Payment.h"

#include <QRandomGenerator>

namespace Payment {

QString momoAccount()
{
    return QStringLiteral("0849739335");
}

QString makeReference(const QString &readerID)
{
    const int suffix = QRandomGenerator::global()->bounded(1000, 10000);
    return QString("%1 %2").arg(readerID).arg(suffix);
}

QString momoPayload(const QString &account, int amount, const QString &reference)
{
    return QString("2|99|%1|||0|0|%2|%3|transfer_p2p")
        .arg(account)
        .arg(amount)
        .arg(reference);
}

} // namespace Payment
