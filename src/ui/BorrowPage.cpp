#include "i18n/Lang.h"
#include "ui/BorrowPage.h"

#include "ui/Theme.h"

#include <QComboBox>
#include <QDate>
#include <QDateEdit>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

BorrowPage::BorrowPage(LibrarySystem *system, QWidget *parent)
    : QWidget(parent), system(system)
{
    auto *layout = new QVBoxLayout(this);
    setObjectName("pageBody");
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);

    // -------------------------------------------------- top: borrow a book
    auto *borrowGroup = new QGroupBox(TR("Borrow a book"), this);
    auto *borrowLayout = new QHBoxLayout(borrowGroup);

    readerCombo = new QComboBox(borrowGroup);
    bookCombo = new QComboBox(borrowGroup);
    borrowDateEdit = new QDateEdit(QDate::currentDate(), borrowGroup);
    borrowDateEdit->setDisplayFormat("yyyy-MM-dd");
    borrowDateEdit->setCalendarPopup(true);
    auto *borrowButton = new QPushButton(TR("Borrow"), borrowGroup);

    borrowLayout->addWidget(new QLabel(TR("Reader:"), borrowGroup));
    borrowLayout->addWidget(readerCombo, 2);
    borrowLayout->addWidget(new QLabel(TR("Book:"), borrowGroup));
    borrowLayout->addWidget(bookCombo, 2);
    borrowLayout->addWidget(new QLabel(TR("Date:"), borrowGroup));
    borrowLayout->addWidget(borrowDateEdit, 1);
    borrowLayout->addWidget(borrowButton);
    layout->addWidget(borrowGroup);

    // ------------------------------------------- reader requests waiting
    auto *requestGroup = new QGroupBox(TR("Reader requests waiting"), this);
    auto *requestLayout = new QVBoxLayout(requestGroup);
    requestTable = new QTableWidget(0, 5, requestGroup);
    requestTable->setHorizontalHeaderLabels({TR("Reservation"), TR("Reader"), TR("Title"),
                                             TR("Requested on"), TR("Status")});
    requestTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    requestTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    requestTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    requestTable->setSelectionMode(QAbstractItemView::SingleSelection);
    requestTable->setAlternatingRowColors(true);
    requestTable->verticalHeader()->setVisible(false);
    requestTable->setMinimumHeight(120);
    requestTable->setMaximumHeight(190);
    requestLayout->addWidget(requestTable);

    auto *requestRow = new QHBoxLayout();
    requestRow->addStretch(1);
    fulfilButton = new QPushButton(TR("Hand over and record the loan"), requestGroup);
    fulfilButton->setEnabled(false);
    requestRow->addWidget(fulfilButton);
    rejectButton = new QPushButton(TR("Reject request"), requestGroup);
    rejectButton->setObjectName("secondaryButton");
    rejectButton->setEnabled(false);
    requestRow->addWidget(rejectButton);
    requestLayout->addLayout(requestRow);
    layout->addWidget(requestGroup);

    // --------------------------------------------- middle: history + filter
    auto *historyGroup = new QGroupBox(TR("Borrow records"), this);
    auto *historyLayout = new QVBoxLayout(historyGroup);

    auto *filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel(TR("Filter:"), historyGroup));
    statusFilter = new QComboBox(historyGroup);
    statusFilter->addItems({TR("All records"), TR("Borrowing"), TR("Returned"), TR("Overdue")});
    filterLayout->addWidget(statusFilter);
    filterLayout->addStretch(1);
    auto *periodLabel = new QLabel(
        QString(TR("Loan period: %1 days · fine %2 per day"))
            .arg(system->loanPeriodDays())
            .arg(LibrarySystem::formatMoney(system->finePerDay())), historyGroup);
    filterLayout->addWidget(periodLabel);
    historyLayout->addLayout(filterLayout);

    table = new QTableWidget(0, 8, historyGroup);
    table->setHorizontalHeaderLabels(
        {TR("Record"), TR("Reader"), TR("Book"), TR("Title"), TR("Borrowed on"),
         TR("Due"), TR("Returned on"), TR("Status")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->setVisible(false);
    table->setMinimumHeight(260);
    historyLayout->addWidget(table, 1);

    detailLabel = new QLabel(TR("Select a record to see its details."), historyGroup);
    detailLabel->setObjectName("detailBox");
    detailLabel->setWordWrap(true);
    detailLabel->setMaximumHeight(96);
    historyLayout->addWidget(detailLabel);

    // ---------------------------------------------- bottom: return a book
    auto *returnLayout = new QHBoxLayout();
    returnLayout->addStretch(1);
    returnLayout->addWidget(new QLabel(TR("Return date:"), historyGroup));
    returnDateEdit = new QDateEdit(QDate::currentDate(), historyGroup);
    returnDateEdit->setDisplayFormat("yyyy-MM-dd");
    returnDateEdit->setCalendarPopup(true);
    returnLayout->addWidget(returnDateEdit);
    returnButton = new QPushButton(TR("Return selected book"), historyGroup);
    returnButton->setEnabled(false);
    returnLayout->addWidget(returnButton);

    renewButton = new QPushButton(TR("Renew"), historyGroup);
    renewButton->setObjectName("secondaryButton");
    renewButton->setEnabled(false);
    returnLayout->addWidget(renewButton);

    settleButton = new QPushButton(TR("Collect fine"), historyGroup);
    settleButton->setObjectName("secondaryButton");
    settleButton->setEnabled(false);
    returnLayout->addWidget(settleButton);

    waiveButton = new QPushButton(TR("Waive fine"), historyGroup);
    waiveButton->setObjectName("secondaryButton");
    waiveButton->setEnabled(false);
    returnLayout->addWidget(waiveButton);

    cancelButton = new QPushButton(TR("Cancel loan"), historyGroup);
    cancelButton->setObjectName("dangerButton");
    cancelButton->setEnabled(false);
    returnLayout->addWidget(cancelButton);
    historyLayout->addLayout(returnLayout);

    layout->addWidget(historyGroup, 1);

    connect(borrowButton, &QPushButton::clicked, this, &BorrowPage::onBorrow);
    connect(fulfilButton, &QPushButton::clicked, this, &BorrowPage::onFulfilRequest);
    connect(rejectButton, &QPushButton::clicked, this, &BorrowPage::onRejectRequest);
    connect(requestTable, &QTableWidget::itemSelectionChanged, this, [this] {
        const bool has = requestTable->currentRow() >= 0;
        fulfilButton->setEnabled(has);
        rejectButton->setEnabled(has);
    });
    connect(returnButton, &QPushButton::clicked, this, &BorrowPage::onReturn);
    connect(renewButton, &QPushButton::clicked, this, &BorrowPage::onRenew);
    connect(cancelButton, &QPushButton::clicked, this, &BorrowPage::onCancelLoan);
    connect(settleButton, &QPushButton::clicked, this, &BorrowPage::onSettleFine);
    connect(waiveButton, &QPushButton::clicked, this, &BorrowPage::onWaiveFine);
    connect(table, &QTableWidget::itemSelectionChanged, this, &BorrowPage::onRowSelected);
    connect(statusFilter, &QComboBox::currentIndexChanged, this, [this](int) { fillTable(); });
    connect(system, &LibrarySystem::dataChanged, this, &BorrowPage::refresh);

    refresh();
}

void BorrowPage::refresh()
{
    // Repopulate the combos (data may have changed on other pages).
    const QString oldReader = readerCombo->currentData().toString();
    const QString oldBook = bookCombo->currentData().toString();

    readerCombo->clear();
    const QList<Reader> readers = system->readerManager()->getAllReaders();
    for (const Reader &r : readers)
        readerCombo->addItem(QString("%1 — %2").arg(r.getReaderID(), r.getFullName()),
                             r.getReaderID());

    bookCombo->clear();
    const QList<Book> books = system->bookManager()->getAllBooks();
    for (const Book &b : books)
        bookCombo->addItem(QString("%1 — %2 (%3)")
                               .arg(b.getBookID(), b.getTitle())
                               .arg(QString(TR("%1 left")).arg(b.getAvailableQuantity())),
                           b.getBookID());

    if (const int i = readerCombo->findData(oldReader); i >= 0)
        readerCombo->setCurrentIndex(i);
    if (const int i = bookCombo->findData(oldBook); i >= 0)
        bookCombo->setCurrentIndex(i);

    fillRequests();
    fillTable();
}

void BorrowPage::fillRequests()
{
    shownRequests.clear();
    requestTable->setRowCount(0);
    const QList<Reservation> all = system->getAllReservations();
    for (const Reservation &r : all) {
        if (!r.isOpen())
            continue;
        shownRequests.append(r);

        const Reader reader = system->readerManager()->findReaderByID(r.getReaderID());
        const Book book = system->bookManager()->findBookByID(r.getBookID());
        const int row = requestTable->rowCount();
        requestTable->insertRow(row);
        requestTable->setItem(row, 0, new QTableWidgetItem(r.getReservationID()));
        requestTable->setItem(row, 1, new QTableWidgetItem(
            reader.isValid() ? QString("%1 — %2").arg(reader.getReaderID(), reader.getFullName())
                             : r.getReaderID()));
        requestTable->setItem(row, 2, new QTableWidgetItem(
            book.isValid() ? book.getTitle() : r.getBookID()));
        requestTable->setItem(row, 3, new QTableWidgetItem(r.getCreatedDate()));

        auto *stateItem = new QTableWidgetItem(r.stateName());
        stateItem->setForeground(QBrush(r.getState() == Reservation::Ready
                                            ? Theme::success() : Theme::warning()));
        requestTable->setItem(row, 4, stateItem);
    }
    fulfilButton->setEnabled(false);
    rejectButton->setEnabled(false);
}

void BorrowPage::onFulfilRequest()
{
    const int row = requestTable->currentRow();
    if (row < 0 || row >= shownRequests.size())
        return;
    // A copy, not a reference: fulfilling emits dataChanged and refills the list.
    const Reservation request = shownRequests.at(row);

    QString error;
    if (!system->fulfilReservation(request.getReservationID(), &error)) {
        QMessageBox::warning(this, TR("Hand over and record the loan"), error);
        return;
    }
    QMessageBox::information(this, TR("Hand over and record the loan"),
                             TR("The loan has been recorded."));
}

void BorrowPage::onRejectRequest()
{
    const int row = requestTable->currentRow();
    if (row < 0 || row >= shownRequests.size())
        return;
    const Reservation request = shownRequests.at(row);

    QString error;
    if (!system->cancelReservation(request.getReservationID(), &error))
        QMessageBox::warning(this, TR("Reject request"), error);
}

void BorrowPage::fillTable()
{
    const int filter = statusFilter->currentIndex(); // 0 all, 1 borrowing, 2 returned, 3 overdue
    const QDate today = QDate::currentDate();
    // Remember what was selected so a refresh does not throw the librarian
    // back to the top of the list after every single action.
    const int prevRow = table->currentRow();
    if (prevRow >= 0 && prevRow < shownRecords.size())
        keepSelectedRecordID = shownRecords.at(prevRow).getRecordID();

    shownRecords.clear();
    table->setRowCount(0);

    const QList<BorrowRecord> records = system->getAllBorrowRecords();
    for (const BorrowRecord &r : records) {
        if (filter == 1 && r.getStatus())
            continue;
        if (filter == 2 && !r.getStatus())
            continue;
        if (filter == 3 && !r.isOverdue(today))
            continue;
        shownRecords.append(r);

        const Book book = system->bookManager()->findBookByID(r.getBookID());
        const Reader reader = system->readerManager()->findReaderByID(r.getReaderID());

        const int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(r.getRecordID()));
        table->setItem(row, 1, new QTableWidgetItem(
            reader.isValid() ? QString("%1 — %2").arg(reader.getReaderID(), reader.getFullName())
                             : r.getReaderID()));
        table->setItem(row, 2, new QTableWidgetItem(r.getBookID()));
        table->setItem(row, 3, new QTableWidgetItem(book.isValid() ? book.getTitle() : "?"));
        table->setItem(row, 4, new QTableWidgetItem(r.getBorrowDate()));
        table->setItem(row, 5, new QTableWidgetItem(
            r.getDueDate().isEmpty() ? QStringLiteral("—") : r.getDueDate()));
        table->setItem(row, 6, new QTableWidgetItem(
            r.getReturnDate().isEmpty() ? QStringLiteral("—") : r.getReturnDate()));

        const int late = r.lateDays(today);
        QString state = r.getStatus() ? TR("Returned") : TR("Borrowing");
        if (r.getStatus() && late > 0) {
            state = QString(TR("Returned late — %1 day(s), fine %2"))
                        .arg(late)
                        .arg(LibrarySystem::formatMoney(r.fine(system->finePerDay(), today)));
        } else if (late > 0) {
            state = QString(TR("OVERDUE by %1 day(s)")).arg(late);
        }

        auto *statusItem = new QTableWidgetItem(state);
        if (late > 0)
            statusItem->setForeground(QBrush(Theme::danger()));
        else
            statusItem->setForeground(QBrush(r.getStatus() ? Theme::success() : Theme::warning()));
        table->setItem(row, 7, statusItem);
    }

    // Put the selection back where it was, if that record is still listed.
    int restore = -1;
    for (int i = 0; i < shownRecords.size(); ++i) {
        if (shownRecords.at(i).getRecordID() == keepSelectedRecordID) {
            restore = i;
            break;
        }
    }
    if (restore >= 0) {
        table->selectRow(restore);
    } else {
        returnButton->setEnabled(false);
        renewButton->setEnabled(false);
        cancelButton->setEnabled(false);
        settleButton->setEnabled(false);
        waiveButton->setEnabled(false);
        detailLabel->setText(TR("Select a record to see its details."));
    }
}

void BorrowPage::onRowSelected()
{
    const int row = table->currentRow();
    if (row < 0 || row >= shownRecords.size()) {
        returnButton->setEnabled(false);
        renewButton->setEnabled(false);
        cancelButton->setEnabled(false);
        settleButton->setEnabled(false);
        waiveButton->setEnabled(false);
        return;
    }
    // A value copy, not a reference: the facade emits dataChanged, which
    // refills shownRecords and would leave a reference dangling.
    const BorrowRecord r = shownRecords.at(row);
    keepSelectedRecordID = r.getRecordID();
    const bool open = !r.getStatus();
    const int owed = r.outstandingFine(system->finePerDay(), QDate::currentDate());
    returnButton->setEnabled(open);
    renewButton->setEnabled(open && !r.isOverdue(QDate::currentDate()));
    cancelButton->setEnabled(open);
    settleButton->setEnabled(owed > 0);
    waiveButton->setEnabled(owed > 0);
    // Polymorphic call through the Entity interface.
    const Entity &entity = r;
    detailLabel->setText(entity.displayInfo());
}

void BorrowPage::onBorrow()
{
    if (readerCombo->currentIndex() < 0 || bookCombo->currentIndex() < 0) {
        QMessageBox::warning(this, TR("Cannot borrow"),
                             TR("Please choose both a reader and a book."));
        return;
    }
    QString error;
    const bool ok = system->borrowBook(readerCombo->currentData().toString(),
                                       bookCombo->currentData().toString(),
                                       borrowDateEdit->date().toString("yyyy-MM-dd"),
                                       &error);
    if (!ok)
        QMessageBox::warning(this, TR("Cannot borrow"), error);
}

void BorrowPage::onReturn()
{
    const int row = table->currentRow();
    if (row < 0 || row >= shownRecords.size())
        return;
    QString error;
    int fine = 0;
    // A value copy, not a reference: the facade emits dataChanged, which
    // refills shownRecords and would leave a reference dangling.
    const BorrowRecord record = shownRecords.at(row);
    const int late = record.lateDays(QDate::currentDate());

    const bool ok = system->returnBook(record.getRecordID(),
                                       returnDateEdit->date().toString("yyyy-MM-dd"),
                                       &error, &fine);
    if (!ok) {
        QMessageBox::warning(this, TR("Cannot return"), error);
        return;
    }

    // Tell the librarian straight away whether money has to be collected —
    // otherwise a late return looks exactly like an on-time one.
    if (fine > 0) {
        QMessageBox::warning(this, TR("Book returned"),
                             QString(TR("Returned %1 day(s) late.\nLate fee to collect: %2"))
                                 .arg(late)
                                 .arg(LibrarySystem::formatMoney(fine)));
    } else {
        QMessageBox::information(this, TR("Book returned"),
                                 TR("Returned on time. Nothing to pay."));
    }
}

// --------------------------------------------------------- extra actions

void BorrowPage::onRenew()
{
    const int row = table->currentRow();
    if (row < 0 || row >= shownRecords.size())
        return;
    QString error;
    const QString id = shownRecords.at(row).getRecordID();
    if (!system->renewLoan(id, &error)) {
        QMessageBox::warning(this, TR("Renew"), error);
        return;
    }
    // Read the record back so the message shows the deadline actually stored.
    const BorrowRecord updated = system->getAllBorrowRecords().isEmpty()
                                     ? BorrowRecord()
                                     : DatabaseManager::instance()->findBorrowRecordByID(id);
    QMessageBox::information(this, TR("Renew"),
                             QString(TR("The loan has been extended to %1."))
                                 .arg(updated.getDueDate()));
}

void BorrowPage::onCancelLoan()
{
    const int row = table->currentRow();
    if (row < 0 || row >= shownRecords.size())
        return;
    const QString id = shownRecords.at(row).getRecordID();
    const auto answer = QMessageBox::question(
        this, TR("Cancel loan"),
        QString(TR("Remove loan \"%1\" as a mistake?\n"
                   "The copy goes straight back on the shelf and the record disappears."))
            .arg(id));
    if (answer != QMessageBox::Yes)
        return;

    QString error;
    if (!system->cancelLoan(id, &error))
        QMessageBox::warning(this, TR("Cancel loan"), error);
}

void BorrowPage::onSettleFine()
{
    const int row = table->currentRow();
    if (row < 0 || row >= shownRecords.size())
        return;
    // A value copy, not a reference: the facade emits dataChanged, which
    // refills shownRecords and would leave a reference dangling.
    const BorrowRecord r = shownRecords.at(row);
    const int owed = r.outstandingFine(system->finePerDay(), QDate::currentDate());

    bool ok = false;
    const int amount = QInputDialog::getInt(
        this, TR("Collect fine"),
        QString(TR("Amount collected (outstanding: %1):"))
            .arg(LibrarySystem::formatMoney(owed)),
        owed, 1, owed, 1000, &ok);
    if (!ok)
        return;

    QString error;
    if (!system->settleFine(r.getRecordID(), amount, &error))
        QMessageBox::warning(this, TR("Collect fine"), error);
}

void BorrowPage::onWaiveFine()
{
    const int row = table->currentRow();
    if (row < 0 || row >= shownRecords.size())
        return;
    // A value copy, not a reference: the facade emits dataChanged, which
    // refills shownRecords and would leave a reference dangling.
    const BorrowRecord r = shownRecords.at(row);
    const int owed = r.outstandingFine(system->finePerDay(), QDate::currentDate());

    const auto answer = QMessageBox::question(
        this, TR("Waive fine"),
        QString(TR("Write off the outstanding %1 on record %2?"))
            .arg(LibrarySystem::formatMoney(owed), r.getRecordID()));
    if (answer != QMessageBox::Yes)
        return;

    QString error;
    if (!system->waiveFine(r.getRecordID(), &error))
        QMessageBox::warning(this, TR("Waive fine"), error);
}
