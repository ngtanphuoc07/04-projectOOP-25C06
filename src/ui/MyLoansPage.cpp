#include "ui/MyLoansPage.h"

#include <QDate>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QStyle>
#include <QTableWidget>
#include <QVBoxLayout>

#include "i18n/Lang.h"
#include "ui/PaymentDialog.h"
#include "ui/SlipDialog.h"
#include "ui/Theme.h"

MyLoansPage::MyLoansPage(LibrarySystem *system, const QString &readerID, QWidget *parent)
    : QWidget(parent), system(system), readerID(readerID)
{
    setObjectName("pageBody");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);

    greetingLabel = new QLabel(this);
    greetingLabel->setObjectName("pageHeading");
    layout->addWidget(greetingLabel);

    auto *cards = new QHBoxLayout();
    cards->setSpacing(14);
    cards->addWidget(makeStatCard(TR("BOOKS IN MY HANDS"), holdingLabel));
    cards->addWidget(makeStatCard(TR("MY BORROW LIMIT"), limitLabel));
    cards->addWidget(makeStatCard(TR("OVERDUE"), overdueLabel));
    cards->addWidget(makeStatCard(TR("MY FINES"), fineLabel));
    layout->addLayout(cards);

    statusLabel = new QLabel(this);
    statusLabel->setObjectName("hintBox");
    statusLabel->setWordWrap(true);
    layout->addWidget(statusLabel);

    auto *currentGroup = new QGroupBox(TR("Books I am holding now"), this);
    auto *currentLayout = new QVBoxLayout(currentGroup);
    currentTable = new QTableWidget(0, 6, currentGroup);
    currentTable->setHorizontalHeaderLabels({TR("Record"), TR("Book"), TR("Title"),
                                             TR("Borrowed on"), TR("Due"), TR("Fine")});
    currentTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    currentTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    currentTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    currentTable->setAlternatingRowColors(true);
    currentTable->verticalHeader()->setVisible(false);
    currentTable->setSortingEnabled(true);
    currentTable->setMinimumHeight(180);
    currentLayout->addWidget(currentTable);
    layout->addWidget(currentGroup, 1);

    auto *historyGroup = new QGroupBox(TR("My borrowing history"), this);
    auto *historyLayout = new QVBoxLayout(historyGroup);
    historyTable = new QTableWidget(0, 5, historyGroup);
    historyTable->setHorizontalHeaderLabels({TR("Record"), TR("Title"), TR("Borrowed on"),
                                             TR("Returned on"), TR("Status")});
    historyTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    historyTable->setAlternatingRowColors(true);
    historyTable->verticalHeader()->setVisible(false);
    historyTable->setSortingEnabled(true);
    historyTable->setMinimumHeight(180);
    historyLayout->addWidget(historyTable);
    layout->addWidget(historyGroup, 1);

    auto *resGroup = new QGroupBox(TR("Titles I am waiting for"), this);
    auto *resLayout = new QVBoxLayout(resGroup);
    reservationTable = new QTableWidget(0, 4, resGroup);
    reservationTable->setHorizontalHeaderLabels({TR("Reservation"), TR("Title"),
                                                 TR("Requested on"), TR("Status")});
    reservationTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    reservationTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    reservationTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    reservationTable->setSelectionMode(QAbstractItemView::SingleSelection);
    reservationTable->setAlternatingRowColors(true);
    reservationTable->verticalHeader()->setVisible(false);
    reservationTable->setMinimumHeight(120);
    reservationTable->setMaximumHeight(190);
    resLayout->addWidget(reservationTable);

    auto *resRow = new QHBoxLayout();
    resRow->addStretch(1);
    payButton = new QPushButton(TR("Pay my fine"), resGroup);
    resRow->addWidget(payButton);

    slipButton = new QPushButton(TR("Print my slip"), resGroup);
    slipButton->setObjectName("secondaryButton");
    resRow->addWidget(slipButton);

    cancelReservationButton = new QPushButton(TR("Cancel reservation"), resGroup);
    cancelReservationButton->setObjectName("secondaryButton");
    resRow->addWidget(cancelReservationButton);
    resLayout->addLayout(resRow);
    layout->addWidget(resGroup);

    connect(cancelReservationButton, &QPushButton::clicked,
            this, &MyLoansPage::onCancelReservation);
    connect(slipButton, &QPushButton::clicked, this, &MyLoansPage::onPrintSlip);
    connect(payButton, &QPushButton::clicked, this, &MyLoansPage::onPay);
    connect(system, &LibrarySystem::dataChanged, this, &MyLoansPage::refresh);
    refresh();
}

void MyLoansPage::fillReservations()
{
    shownReservations = system->getOpenReservationsByReader(readerID);
    reservationTable->setRowCount(0);
    for (const Reservation &r : shownReservations) {
        const Book book = system->bookManager()->findBookByID(r.getBookID());
        const int row = reservationTable->rowCount();
        reservationTable->insertRow(row);
        reservationTable->setItem(row, 0, new QTableWidgetItem(r.getReservationID()));
        reservationTable->setItem(row, 1, new QTableWidgetItem(
            book.isValid() ? book.getTitle() : r.getBookID()));
        reservationTable->setItem(row, 2, new QTableWidgetItem(r.getCreatedDate()));

        auto *stateItem = new QTableWidgetItem(r.stateName());
        // "Ready for pickup" is the one a reader has to act on.
        stateItem->setForeground(QBrush(r.getState() == Reservation::Ready
                                            ? Theme::success() : Theme::muted()));
        reservationTable->setItem(row, 3, stateItem);
    }
    cancelReservationButton->setEnabled(!shownReservations.isEmpty());
}

void MyLoansPage::onPay()
{
    PaymentDialog dialog(system, readerID, this);
    dialog.exec();
}

void MyLoansPage::onPrintSlip()
{
    SlipDialog dialog(system, readerID, this);
    dialog.exec();
}

void MyLoansPage::onCancelReservation()
{
    const int row = reservationTable->currentRow();
    if (row < 0 || row >= shownReservations.size())
        return;
    QString error;
    if (!system->cancelReservation(shownReservations.at(row).getReservationID(), &error))
        QMessageBox::warning(this, TR("Cancel reservation"), error);
}

QWidget *MyLoansPage::makeStatCard(const QString &title, QLabel *&numberLabel)
{
    auto *card = new QFrame(this);
    card->setObjectName("statCard");
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(15, 13, 15, 13);
    cardLayout->setSpacing(3);

    numberLabel = new QLabel("0", card);
    numberLabel->setObjectName("statNumber");
    auto *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("statTitle");
    titleLabel->setWordWrap(true);

    cardLayout->addWidget(numberLabel);
    cardLayout->addWidget(titleLabel);
    return card;
}

void MyLoansPage::refresh()
{
    const Reader me = system->readerManager()->findReaderByID(readerID);
    greetingLabel->setText(me.isValid()
                               ? QString("%1  (%2)")
                                     .arg(QString(TR("Hello, %1")).arg(me.getFullName()), readerID)
                               : QString("%1 %2").arg(TR("Reader"), readerID));

    const QList<BorrowRecord> active = system->getActiveRecordsByReader(readerID);
    const QList<BorrowRecord> history = system->getRecordsByReader(readerID);

    const QDate today = QDate::currentDate();
    int overdue = 0;
    for (const BorrowRecord &r : active) {
        if (r.isOverdue(today))
            ++overdue;
    }
    const int fines = system->outstandingFine(readerID);

    holdingLabel->setText(QString::number(active.size()));
    limitLabel->setText(QString::number(system->maxBooksPerReader()));
    overdueLabel->setText(QString::number(overdue));
    fineLabel->setText(LibrarySystem::formatMoney(fines));

    overdueLabel->setStyleSheet(overdue > 0 ? "color:" + Theme::danger().name() + ";" : QString());
    fineLabel->setStyleSheet(fines > 0 ? "color:" + Theme::danger().name() + ";" : QString());

    // One clear sentence about whether the reader may borrow right now.
    if (overdue > 0) {
        statusLabel->setText(TR("You have an overdue book. Please return it before borrowing again."));
        statusLabel->setObjectName("warnBox");
    } else if (active.size() >= system->maxBooksPerReader()) {
        statusLabel->setText(QString(TR("You are holding %1 book(s); the limit is %2."))
                                 .arg(active.size()).arg(system->maxBooksPerReader()));
        statusLabel->setObjectName("warnBox");
    } else {
        statusLabel->setText(TR("Everything you borrowed is within its deadline."));
        statusLabel->setObjectName("hintBox");
    }
    statusLabel->style()->unpolish(statusLabel);
    statusLabel->style()->polish(statusLabel);

    payButton->setEnabled(fines > 0);
    payButton->setToolTip(fines > 0 ? QString(TR("You owe %1"))
                                          .arg(LibrarySystem::formatMoney(fines))
                                    : TR("Nothing to pay."));

    fillCurrent(active);
    fillHistory(history);
    fillReservations();
}

void MyLoansPage::fillCurrent(const QList<BorrowRecord> &records)
{
    const QDate today = QDate::currentDate();
    const int perDay = system->finePerDay();
    currentTable->setSortingEnabled(false);
    currentTable->setRowCount(0);

    for (const BorrowRecord &r : records) {
        const Book book = system->bookManager()->findBookByID(r.getBookID());
        const int row = currentTable->rowCount();
        currentTable->insertRow(row);
        currentTable->setItem(row, 0, new QTableWidgetItem(r.getRecordID()));
        currentTable->setItem(row, 1, new QTableWidgetItem(r.getBookID()));
        currentTable->setItem(row, 2, new QTableWidgetItem(book.isValid() ? book.getTitle() : "?"));
        currentTable->setItem(row, 3, new QTableWidgetItem(r.getBorrowDate()));

        const int late = r.lateDays(today);
        auto *dueItem = new QTableWidgetItem(
            late > 0 ? QString(TR("%1  (late by %2 day(s))")).arg(r.getDueDate()).arg(late)
                     : r.getDueDate());
        if (late > 0)
            dueItem->setForeground(QBrush(Theme::danger()));
        currentTable->setItem(row, 4, dueItem);

        auto *fineItem = new QTableWidgetItem(LibrarySystem::formatMoney(r.fine(perDay, today)));
        if (late > 0)
            fineItem->setForeground(QBrush(Theme::danger()));
        currentTable->setItem(row, 5, fineItem);
    }
    currentTable->setSortingEnabled(true);
}

void MyLoansPage::fillHistory(const QList<BorrowRecord> &records)
{
    const QDate today = QDate::currentDate();
    const int lostAfter = system->lostAfterDays();
    historyTable->setSortingEnabled(false);
    historyTable->setRowCount(0);

    for (const BorrowRecord &r : records) {
        const Book book = system->bookManager()->findBookByID(r.getBookID());
        const int row = historyTable->rowCount();
        historyTable->insertRow(row);
        historyTable->setItem(row, 0, new QTableWidgetItem(r.getRecordID()));
        historyTable->setItem(row, 1, new QTableWidgetItem(book.isValid() ? book.getTitle() : "?"));
        historyTable->setItem(row, 2, new QTableWidgetItem(r.getBorrowDate()));
        historyTable->setItem(row, 3, new QTableWidgetItem(
            r.getReturnDate().isEmpty() ? QStringLiteral("—") : r.getReturnDate()));

        const int late = r.lateDays(today);
        QString state;
        QColor colour;
        if (r.getStatus() && late > 0) {
            state = QString(TR("Returned late — %1 day(s), fine %2"))
                        .arg(late)
                        .arg(LibrarySystem::formatMoney(r.fine(system->finePerDay(), today)));
            colour = Theme::danger();
        } else if (r.getStatus()) {
            state = TR("Returned");
            colour = Theme::success();
        } else if (r.isLost(today, lostAfter)) {
            state = QString(TR("NOT RETURNED — %1 day(s) late")).arg(late);
            colour = Theme::critical();
        } else if (late > 0) {
            state = QString(TR("OVERDUE by %1 day(s)")).arg(late);
            colour = Theme::danger();
        } else {
            state = TR("Borrowing");
            colour = Theme::warning();
        }

        auto *statusItem = new QTableWidgetItem(state);
        statusItem->setForeground(QBrush(colour));
        historyTable->setItem(row, 4, statusItem);
    }
    historyTable->setSortingEnabled(true);
}
