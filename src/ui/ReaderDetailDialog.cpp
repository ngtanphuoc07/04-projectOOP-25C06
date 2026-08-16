#include "ui/ReaderDetailDialog.h"

#include <QDate>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "i18n/Lang.h"
#include "ui/Theme.h"
#include "util/AvatarUtil.h"

ReaderDetailDialog::ReaderDetailDialog(LibrarySystem *system, const QString &readerID,
                                       bool allowPhotoChange, QWidget *parent)
    : QDialog(parent), system(system), readerID(readerID),
      allowPhotoChange(allowPhotoChange)
{
    setWindowTitle(TR("Reader details"));
    resize(760, 620);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(14);

    layout->addWidget(buildHeader());

    auto *loansGroup = new QGroupBox(TR("Borrowing history"), this);
    auto *loansLayout = new QVBoxLayout(loansGroup);
    loansTable = new QTableWidget(0, 6, loansGroup);
    loansTable->setHorizontalHeaderLabels({TR("Record"), TR("Title"), TR("Borrowed on"),
                                           TR("Due"), TR("Status"), TR("Fine")});
    loansTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    loansTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    loansTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    loansTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    loansTable->setAlternatingRowColors(true);
    loansTable->verticalHeader()->setVisible(false);
    loansTable->setSortingEnabled(true);
    loansLayout->addWidget(loansTable);
    layout->addWidget(loansGroup, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->button(QDialogButtonBox::Close)->setText(TR("Close"));
    layout->addWidget(buttons);

    setStyleSheet(Theme::dialogStyleSheet());

    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(system, &LibrarySystem::dataChanged, this, &ReaderDetailDialog::refresh);

    refresh();
}

QWidget *ReaderDetailDialog::buildHeader()
{
    auto *card = new QGroupBox(this);
    auto *row = new QHBoxLayout(card);
    row->setContentsMargins(14, 12, 14, 12);
    row->setSpacing(18);

    auto *photoColumn = new QVBoxLayout();
    photoLabel = new QLabel(card);
    photoLabel->setFixedSize(128, 128);
    photoColumn->addWidget(photoLabel);

    if (allowPhotoChange) {
        auto *change = new QPushButton(TR("Change photo"), card);
        change->setObjectName("secondaryButton");
        auto *remove = new QPushButton(TR("Remove photo"), card);
        remove->setObjectName("secondaryButton");
        photoColumn->addWidget(change);
        photoColumn->addWidget(remove);
        connect(change, &QPushButton::clicked, this, &ReaderDetailDialog::onChangePhoto);
        connect(remove, &QPushButton::clicked, this, &ReaderDetailDialog::onRemovePhoto);
    }
    photoColumn->addStretch(1);
    row->addLayout(photoColumn);

    auto *textColumn = new QVBoxLayout();
    textColumn->setSpacing(5);
    nameLabel = new QLabel(card);
    nameLabel->setObjectName("dialogHeading");
    idLabel = new QLabel(card);
    idLabel->setObjectName("dialogSummary");
    contactLabel = new QLabel(card);
    contactLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    statsLabel = new QLabel(card);
    statsLabel->setWordWrap(true);

    textColumn->addWidget(nameLabel);
    textColumn->addWidget(idLabel);
    textColumn->addSpacing(6);
    textColumn->addWidget(contactLabel);
    textColumn->addSpacing(6);
    textColumn->addWidget(statsLabel);
    textColumn->addStretch(1);
    row->addLayout(textColumn, 1);

    return card;
}

void ReaderDetailDialog::onChangePhoto()
{
    QString error;
    const QByteArray png = AvatarUtil::pickImage(this, &error);
    if (png.isEmpty()) {
        if (!error.isEmpty())
            QMessageBox::warning(this, TR("Change photo"), error);
        return;                          // cancelled
    }
    if (!system->readerManager()->updateAvatar(readerID, png, &error)) {
        QMessageBox::warning(this, TR("Change photo"), error);
        return;
    }
    system->audit("AVATAR", QString("reader %1").arg(readerID));
    system->notifyDataChanged();
}

void ReaderDetailDialog::onRemovePhoto()
{
    QString error;
    if (!system->readerManager()->updateAvatar(readerID, QByteArray(), &error)) {
        QMessageBox::warning(this, TR("Remove photo"), error);
        return;
    }
    system->notifyDataChanged();
}

void ReaderDetailDialog::refresh()
{
    const Reader r = system->readerManager()->findReaderByID(readerID);
    if (!r.isValid()) {
        nameLabel->setText(TR("This reader no longer exists."));
        return;
    }

    photoLabel->setPixmap(AvatarUtil::toPixmap(r.getAvatar(), 128, r.getFullName()));
    nameLabel->setText(r.getFullName());
    idLabel->setText(QString(TR("Reader card %1")).arg(r.getReaderID()));
    contactLabel->setText(QString("%1: %2<br/>%3: %4")
                              .arg(TR("Phone"), r.getPhone().isEmpty() ? "—" : r.getPhone(),
                                   TR("Email"), r.getEmail().isEmpty() ? "—" : r.getEmail()));

    const QDate today = QDate::currentDate();
    const QList<BorrowRecord> active = system->getActiveRecordsByReader(readerID);
    const QList<BorrowRecord> history = system->getRecordsByReader(readerID);
    const int owed = system->outstandingFine(readerID);
    int overdue = 0;
    for (const BorrowRecord &rec : active) {
        if (rec.isOverdue(today))
            ++overdue;
    }

    QStringList bits;
    bits << QString(TR("Holding %1 of %2 books"))
                .arg(active.size()).arg(system->maxBooksPerReader());
    bits << QString(TR("%1 loan(s) in total")).arg(history.size());
    if (overdue > 0)
        bits << QString("<font color='%1'><b>%2</b></font>")
                    .arg(Theme::danger().name(),
                         QString(TR("%1 overdue")).arg(overdue));
    if (owed > 0)
        bits << QString("<font color='%1'><b>%2</b></font>")
                    .arg(Theme::danger().name(),
                         QString(TR("owes %1")).arg(LibrarySystem::formatMoney(owed)));
    statsLabel->setText(bits.join(" &nbsp;·&nbsp; "));

    const int perDay = system->finePerDay();
    const int lostAfter = system->lostAfterDays();
    loansTable->setSortingEnabled(false);
    loansTable->setRowCount(0);
    for (const BorrowRecord &rec : history) {
        const Book book = system->bookManager()->findBookByID(rec.getBookID());
        const int row = loansTable->rowCount();
        loansTable->insertRow(row);
        loansTable->setItem(row, 0, new QTableWidgetItem(rec.getRecordID()));
        loansTable->setItem(row, 1, new QTableWidgetItem(
            book.isValid() ? book.getTitle() : rec.getBookID()));
        loansTable->setItem(row, 2, new QTableWidgetItem(rec.getBorrowDate()));
        loansTable->setItem(row, 3, new QTableWidgetItem(
            rec.getDueDate().isEmpty() ? QStringLiteral("—") : rec.getDueDate()));

        const int late = rec.lateDays(today);
        QString state;
        QColor colour;
        if (rec.getStatus() && late > 0) {
            state = QString(TR("Returned late — %1 day(s)")).arg(late);
            colour = Theme::danger();
        } else if (rec.getStatus()) {
            state = TR("Returned");
            colour = Theme::success();
        } else if (rec.isLost(today, lostAfter)) {
            state = QString(TR("NOT RETURNED — %1 day(s) late")).arg(late);
            colour = Theme::critical();
        } else if (late > 0) {
            state = QString(TR("OVERDUE by %1 day(s)")).arg(late);
            colour = Theme::danger();
        } else {
            state = TR("Borrowing");
            colour = Theme::warning();
        }
        auto *stateItem = new QTableWidgetItem(state);
        stateItem->setForeground(QBrush(colour));
        loansTable->setItem(row, 4, stateItem);

        auto *fineItem = new QTableWidgetItem(
            LibrarySystem::formatMoney(rec.fine(perDay, today)));
        if (late > 0)
            fineItem->setForeground(QBrush(Theme::danger()));
        loansTable->setItem(row, 5, fineItem);
    }
    loansTable->setSortingEnabled(true);
}
