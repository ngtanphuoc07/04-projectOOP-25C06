#include "ui/DashboardPage.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QTableWidget>
#include <QVBoxLayout>

DashboardPage::DashboardPage(LibrarySystem *system, QWidget *parent)
    : QWidget(parent), system(system)
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    auto *cards = new QHBoxLayout();
    cards->setSpacing(14);
    cards->addWidget(makeStatCard("TOTAL BOOK COPIES", totalBooksLabel));
    cards->addWidget(makeStatCard("REGISTERED READERS", totalReadersLabel));
    cards->addWidget(makeStatCard("COPIES BORROWED", borrowedLabel));
    cards->addWidget(makeStatCard("COPIES ON SHELF", availableLabel));
    layout->addLayout(cards);

    auto *group = new QGroupBox("Books currently borrowed", this);
    auto *groupLayout = new QVBoxLayout(group);
    activeTable = new QTableWidget(0, 5, group);
    activeTable->setHorizontalHeaderLabels({"Record", "Reader", "Book", "Title", "Borrowed on"});
    activeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    activeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    activeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    activeTable->verticalHeader()->setVisible(false);
    groupLayout->addWidget(activeTable);
    layout->addWidget(group, 1);

    connect(system, &LibrarySystem::dataChanged, this, &DashboardPage::refresh);
    refresh();
}

QWidget *DashboardPage::makeStatCard(const QString &title, QLabel *&numberLabel)
{
    auto *card = new QFrame(this);
    card->setObjectName("statCard");
    auto *cardLayout = new QVBoxLayout(card);
    cardLayout->setContentsMargins(16, 14, 16, 14);

    numberLabel = new QLabel("0", card);
    numberLabel->setObjectName("statNumber");
    auto *titleLabel = new QLabel(title, card);
    titleLabel->setObjectName("statTitle");

    cardLayout->addWidget(numberLabel);
    cardLayout->addWidget(titleLabel);
    return card;
}

void DashboardPage::refresh()
{
    totalBooksLabel->setText(QString::number(system->countTotalBooks()));
    totalReadersLabel->setText(QString::number(system->countTotalReaders()));
    borrowedLabel->setText(QString::number(system->countBorrowedBooks()));
    availableLabel->setText(QString::number(system->countAvailableBooks()));

    activeTable->setRowCount(0);
    const QList<BorrowRecord> records = system->getAllBorrowRecords();
    for (const BorrowRecord &r : records) {
        if (r.getStatus())
            continue; // only books still out
        const Book book = system->bookManager()->findBookByID(r.getBookID());
        const Reader reader = system->readerManager()->findReaderByID(r.getReaderID());

        const int row = activeTable->rowCount();
        activeTable->insertRow(row);
        activeTable->setItem(row, 0, new QTableWidgetItem(r.getRecordID()));
        activeTable->setItem(row, 1, new QTableWidgetItem(
            reader.isValid() ? QString("%1 — %2").arg(reader.getReaderID(), reader.getFullName())
                             : r.getReaderID()));
        activeTable->setItem(row, 2, new QTableWidgetItem(r.getBookID()));
        activeTable->setItem(row, 3, new QTableWidgetItem(book.isValid() ? book.getTitle() : "?"));
        activeTable->setItem(row, 4, new QTableWidgetItem(r.getBorrowDate()));
    }
}
