#include "ui/BorrowPage.h"

#include <QComboBox>
#include <QDateEdit>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

BorrowPage::BorrowPage(LibrarySystem *system, QWidget *parent)
    : QWidget(parent), system(system)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    // -------------------------------------------------- top: borrow a book
    auto *borrowGroup = new QGroupBox("Borrow a book", this);
    auto *borrowLayout = new QHBoxLayout(borrowGroup);

    readerCombo = new QComboBox(borrowGroup);
    bookCombo = new QComboBox(borrowGroup);
    borrowDateEdit = new QDateEdit(QDate::currentDate(), borrowGroup);
    borrowDateEdit->setDisplayFormat("yyyy-MM-dd");
    borrowDateEdit->setCalendarPopup(true);
    auto *borrowButton = new QPushButton("Borrow", borrowGroup);

    borrowLayout->addWidget(new QLabel("Reader:", borrowGroup));
    borrowLayout->addWidget(readerCombo, 2);
    borrowLayout->addWidget(new QLabel("Book:", borrowGroup));
    borrowLayout->addWidget(bookCombo, 2);
    borrowLayout->addWidget(new QLabel("Date:", borrowGroup));
    borrowLayout->addWidget(borrowDateEdit, 1);
    borrowLayout->addWidget(borrowButton);
    layout->addWidget(borrowGroup);

    // --------------------------------------------- middle: history + filter
    auto *historyGroup = new QGroupBox("Borrow records", this);
    auto *historyLayout = new QVBoxLayout(historyGroup);

    auto *filterLayout = new QHBoxLayout();
    filterLayout->addWidget(new QLabel("Filter:", historyGroup));
    statusFilter = new QComboBox(historyGroup);
    statusFilter->addItems({"All records", "Borrowing", "Returned"});
    filterLayout->addWidget(statusFilter);
    filterLayout->addStretch(1);
    historyLayout->addLayout(filterLayout);

    table = new QTableWidget(0, 7, historyGroup);
    table->setHorizontalHeaderLabels(
        {"Record", "Reader", "Book", "Title", "Borrowed on", "Returned on", "Status"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->setVisible(false);
    historyLayout->addWidget(table, 1);

    detailLabel = new QLabel("Select a record to see its details.", historyGroup);
    detailLabel->setObjectName("detailBox");
    historyLayout->addWidget(detailLabel);

    // ---------------------------------------------- bottom: return a book
    auto *returnLayout = new QHBoxLayout();
    returnLayout->addStretch(1);
    returnLayout->addWidget(new QLabel("Return date:", historyGroup));
    returnDateEdit = new QDateEdit(QDate::currentDate(), historyGroup);
    returnDateEdit->setDisplayFormat("yyyy-MM-dd");
    returnDateEdit->setCalendarPopup(true);
    returnLayout->addWidget(returnDateEdit);
    returnButton = new QPushButton("Return selected book", historyGroup);
    returnButton->setEnabled(false);
    returnLayout->addWidget(returnButton);
    historyLayout->addLayout(returnLayout);

    layout->addWidget(historyGroup, 1);

    connect(borrowButton, &QPushButton::clicked, this, &BorrowPage::onBorrow);
    connect(returnButton, &QPushButton::clicked, this, &BorrowPage::onReturn);
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
        bookCombo->addItem(QString("%1 — %2 (%3 left)")
                               .arg(b.getBookID(), b.getTitle())
                               .arg(b.getAvailableQuantity()),
                           b.getBookID());

    if (const int i = readerCombo->findData(oldReader); i >= 0)
        readerCombo->setCurrentIndex(i);
    if (const int i = bookCombo->findData(oldBook); i >= 0)
        bookCombo->setCurrentIndex(i);

    fillTable();
}

void BorrowPage::fillTable()
{
    const int filter = statusFilter->currentIndex(); // 0 all, 1 borrowing, 2 returned
    shownRecords.clear();
    table->setRowCount(0);

    const QList<BorrowRecord> records = system->getAllBorrowRecords();
    for (const BorrowRecord &r : records) {
        if (filter == 1 && r.getStatus())
            continue;
        if (filter == 2 && !r.getStatus())
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
            r.getReturnDate().isEmpty() ? QStringLiteral("—") : r.getReturnDate()));

        auto *statusItem = new QTableWidgetItem(r.getStatus() ? "Returned" : "Borrowing");
        statusItem->setForeground(r.getStatus() ? QBrush(QColor("#2e7d32"))
                                                : QBrush(QColor("#b26a00")));
        table->setItem(row, 6, statusItem);
    }

    returnButton->setEnabled(false);
    detailLabel->setText("Select a record to see its details.");
}

void BorrowPage::onRowSelected()
{
    const int row = table->currentRow();
    if (row < 0 || row >= shownRecords.size()) {
        returnButton->setEnabled(false);
        return;
    }
    const BorrowRecord &r = shownRecords.at(row);
    returnButton->setEnabled(!r.getStatus());
    // Polymorphic call through the Entity interface.
    const Entity &entity = r;
    detailLabel->setText(entity.displayInfo());
}

void BorrowPage::onBorrow()
{
    if (readerCombo->currentIndex() < 0 || bookCombo->currentIndex() < 0) {
        QMessageBox::warning(this, "Cannot borrow", "Please choose both a reader and a book.");
        return;
    }
    QString error;
    const bool ok = system->borrowBook(readerCombo->currentData().toString(),
                                       bookCombo->currentData().toString(),
                                       borrowDateEdit->date().toString("yyyy-MM-dd"),
                                       &error);
    if (!ok)
        QMessageBox::warning(this, "Cannot borrow", error);
}

void BorrowPage::onReturn()
{
    const int row = table->currentRow();
    if (row < 0 || row >= shownRecords.size())
        return;
    QString error;
    const bool ok = system->returnBook(shownRecords.at(row).getRecordID(),
                                       returnDateEdit->date().toString("yyyy-MM-dd"),
                                       &error);
    if (!ok)
        QMessageBox::warning(this, "Cannot return", error);
}
