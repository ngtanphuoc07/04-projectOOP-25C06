#include "ui/PaymentDialog.h"

#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QTabWidget>
#include <QVBoxLayout>

#include "i18n/Lang.h"
#include "ui/Theme.h"
#include "util/Payment.h"
#include "util/QrCode.h"

PaymentDialog::PaymentDialog(LibrarySystem *system, const QString &readerID, QWidget *parent)
    : QDialog(parent), system(system), readerID(readerID),
      amount(system->outstandingFine(readerID))
{
    setWindowTitle(TR("Pay my fine"));
    setMinimumWidth(560);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(12);

    auto *heading = new QLabel(
        QString(TR("You owe %1")).arg(LibrarySystem::formatMoney(amount)), this);
    heading->setObjectName("dialogHeading");
    layout->addWidget(heading);

    auto *note = new QLabel(
        TR("Paying here does not clear the fine by itself. A librarian confirms "
           "the payment and records it, which is when your balance drops."), this);
    note->setObjectName("hintBox");
    note->setWordWrap(true);
    layout->addWidget(note);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(buildMomoTab(), TR("Transfer by MoMo"));
    tabs->addTab(buildCounterTab(), TR("Pay at the counter"));
    layout->addWidget(tabs, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(TR("Close"));
    layout->addWidget(buttons);

    setStyleSheet(Theme::dialogStyleSheet());
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    refreshQr();
}

QWidget *PaymentDialog::buildMomoTab()
{
    auto *page = new QWidget(this);
    auto *outer = new QHBoxLayout(page);
    outer->setContentsMargins(14, 14, 14, 14);
    outer->setSpacing(18);

    qrLabel = new QLabel(page);
    qrLabel->setFixedSize(240, 240);
    qrLabel->setAlignment(Qt::AlignCenter);
    outer->addWidget(qrLabel);

    auto *right = new QVBoxLayout();
    right->setSpacing(9);

    auto *form = new QFormLayout();
    referenceEdit = new QLineEdit(Payment::makeReference(readerID), page);
    form->addRow(TR("Transfer reference:"), referenceEdit);
    right->addLayout(form);

    detailsLabel = new QLabel(page);
    detailsLabel->setObjectName("detailBox");
    detailsLabel->setWordWrap(true);
    detailsLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    right->addWidget(detailsLabel);

    auto *saveButton = new QPushButton(TR("Save the QR as an image"), page);
    saveButton->setObjectName("secondaryButton");
    right->addWidget(saveButton);

    auto *warn = new QLabel(
        TR("This code does not carry the amount. After scanning, type the amount "
           "and the reference above into your banking app."), page);
    warn->setObjectName("warnBox");
    warn->setWordWrap(true);
    right->addWidget(warn);
    right->addStretch(1);

    outer->addLayout(right, 1);

    connect(referenceEdit, &QLineEdit::textChanged, this, &PaymentDialog::refreshQr);
    connect(saveButton, &QPushButton::clicked, this, &PaymentDialog::onSaveQr);
    return page;
}

QWidget *PaymentDialog::buildCounterTab()
{
    auto *page = new QWidget(this);
    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(14, 14, 14, 14);
    outer->setSpacing(12);

    auto *box = new QLabel(
        QString(TR("Bring %1 in cash to the library desk.\n\n"
                   "Tell the librarian your reader card number (%2). They will "
                   "record the payment straight away and your balance updates here."))
            .arg(LibrarySystem::formatMoney(amount), readerID), page);
    box->setObjectName("detailBox");
    box->setWordWrap(true);
    outer->addWidget(box);

    auto *tip = new QLabel(
        TR("Use \"Print my slip\" on the My books screen to take a printed "
           "summary with you."), page);
    tip->setObjectName("hintBox");
    tip->setWordWrap(true);
    outer->addWidget(tip);
    outer->addStretch(1);

    return page;
}

QString PaymentDialog::momoPayload() const
{
    // MoMo's personal-transfer QR format is not published, so the payload uses
    // the widely-cited layout AND every field is shown as plain text beside the
    // code — a reader whose app rejects the scan can still transfer by hand.
    return Payment::momoPayload(Payment::momoAccount(), amount,
                                referenceEdit->text().trimmed());
}

void PaymentDialog::refreshQr()
{
    // The library's own uploaded code wins: it is the one the money actually
    // reaches. Only when none has been uploaded do we draw one.
    const QByteArray uploaded = system->paymentQrImage();
    if (!uploaded.isEmpty()) {
        QPixmap pm;
        if (pm.loadFromData(uploaded, "PNG")) {
            qrLabel->setPixmap(pm.scaled(232, 232, Qt::KeepAspectRatio,
                                         Qt::SmoothTransformation));
        }
    } else {
        const QrCode code = QrCode::encode(momoPayload().toUtf8());
        if (code.isValid()) {
            qrLabel->setPixmap(QPixmap::fromImage(
                code.toImage(6, 3).scaled(232, 232, Qt::KeepAspectRatio,
                                          Qt::FastTransformation)));
        } else {
            qrLabel->setText(TR("The reference is too long for a QR code."));
        }
    }

    detailsLabel->setText(
        QString("<b>%1:</b> %2<br/><b>%3:</b> %4<br/><b>%5:</b> %6")
            .arg(TR("MoMo number"), Payment::momoAccount(),
                 TR("Amount"), LibrarySystem::formatMoney(amount),
                 TR("Reference"), referenceEdit->text().trimmed().toHtmlEscaped()));
}

void PaymentDialog::onSaveQr()
{
    const QrCode code = QrCode::encode(momoPayload().toUtf8());
    if (!code.isValid() && system->paymentQrImage().isEmpty())
        return;

    const QString path = QFileDialog::getSaveFileName(
        this, TR("Save the QR as an image"),
        QString("momo-%1.png").arg(readerID),
        TR("Images (*.png)"));
    if (path.isEmpty())
        return;

    const QByteArray uploaded = system->paymentQrImage();
    if (!uploaded.isEmpty()) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly) || file.write(uploaded) < 0) {
            QMessageBox::warning(this, TR("Save the QR as an image"),
                                 QString(TR("Could not write to \"%1\".")).arg(path));
            return;
        }
        file.close();
        QMessageBox::information(this, TR("Save the QR as an image"),
                                 QString(TR("Saved to:\n%1")).arg(path));
        return;
    }

    if (!code.toImage(10, 4).save(path)) {
        QMessageBox::warning(this, TR("Save the QR as an image"),
                             QString(TR("Could not write to \"%1\".")).arg(path));
        return;
    }
    QMessageBox::information(this, TR("Save the QR as an image"),
                             QString(TR("Saved to:\n%1")).arg(path));
}
