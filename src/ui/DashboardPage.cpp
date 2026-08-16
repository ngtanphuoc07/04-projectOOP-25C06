#include "ui/DashboardPage.h"

#include <QDate>
#include <QFrame>
#include <QComboBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QStyle>
#include <QTableWidget>
#include <QVBoxLayout>

#include "i18n/Lang.h"
#include "ui/Exporter.h"
#include "ui/Theme.h"

DashboardPage::DashboardPage(LibrarySystem *system, QWidget *parent)
    : QWidget(parent), system(system)
{
    setObjectName("pageBody");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(16);

    auto *cards = new QHBoxLayout();
    cards->setSpacing(14);
    cards->addWidget(makeStatCard(TR("TOTAL BOOK COPIES"), totalBooksLabel));
    cards->addWidget(makeStatCard(TR("REGISTERED READERS"), totalReadersLabel));
    cards->addWidget(makeStatCard(TR("COPIES BORROWED"), borrowedLabel));
    cards->addWidget(makeStatCard(TR("COPIES ON SHELF"), availableLabel));
    cards->addWidget(makeStatCard(TR("OVERDUE"), overdueLabel));
    cards->addWidget(makeStatCard(TR("NOT RETURNED"), lostLabel));
    cards->addWidget(makeStatCard(TR("FINES DUE"), fineLabel));
    layout->addLayout(cards);

    // A single line telling the librarian whether anything needs chasing.
    alertLabel = new QLabel(this);
    alertLabel->setObjectName("hintBox");
    alertLabel->setWordWrap(true);
    layout->addWidget(alertLabel);

    auto *group = new QGroupBox(TR("Books currently borrowed"), this);
    auto *groupLayout = new QVBoxLayout(group);

    // The table always lists every copy that is out; the filter narrows it to
    // the ones that need chasing.
    auto *filterRow = new QHBoxLayout();
    filterRow->addWidget(new QLabel(TR("Filter:"), group));
    loanFilter = new QComboBox(group);
    loanFilter->addItems({TR("All books on loan"), TR("Overdue"), TR("Not returned")});
    filterRow->addWidget(loanFilter);
    filterRow->addStretch(1);
    groupLayout->addLayout(filterRow);
    connect(loanFilter, &QComboBox::currentIndexChanged, this, [this](int) { refresh(); });
    activeTable = new QTableWidget(0, 8, group);
    activeTable->setHorizontalHeaderLabels(
        {TR("Record"), TR("Reader"), TR("Book"), TR("Title"), TR("Borrowed on"),
         TR("Due"), TR("Status"), TR("Fine")});
    activeTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    activeTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    activeTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    activeTable->setAlternatingRowColors(true);
    activeTable->verticalHeader()->setVisible(false);
    activeTable->setSortingEnabled(true);
    activeTable->setMinimumHeight(300);
    groupLayout->addWidget(activeTable);

    auto *exportRow = new QHBoxLayout();
    exportRow->addStretch(1);
    auto *exportButton = new QPushButton(TR("Export to CSV"), group);
    exportButton->setObjectName("secondaryButton");
    exportRow->addWidget(exportButton);
    groupLayout->addLayout(exportRow);

    layout->addWidget(group, 1);

    connect(exportButton, &QPushButton::clicked, this, [this] {
        Exporter::exportTableToCsv(activeTable, "books-on-loan", this);
    });

    connect(system, &LibrarySystem::dataChanged, this, &DashboardPage::refresh);
    refresh();
}

QWidget *DashboardPage::makeStatCard(const QString &title, QLabel *&numberLabel)
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

void DashboardPage::refresh()
{
    // One pass produces every number below; the old code scanned the whole
    // database once per statistic.
    const LibraryStats st = system->stats();
    totalBooksLabel->setText(QString::number(st.totalCopies));
    totalReadersLabel->setText(QString::number(st.readers));
    borrowedLabel->setText(QString::number(st.borrowed));
    availableLabel->setText(QString::number(st.available));

    const int overdue = st.overdue;
    const int lost = st.notReturned;
    const int fines = st.finesDue;

    overdueLabel->setText(QString::number(overdue));
    lostLabel->setText(QString::number(lost));
    fineLabel->setText(LibrarySystem::formatMoney(fines));

    // The numbers a librarian has to act on go red; the rest stay neutral.
    overdueLabel->setStyleSheet(overdue > 0 ? "color:" + Theme::danger().name() + ";" : QString());
    lostLabel->setStyleSheet(lost > 0 ? "color:" + Theme::critical().name() + ";" : QString());
    fineLabel->setStyleSheet(fines > 0 ? "color:" + Theme::danger().name() + ";" : QString());

    if (overdue == 0 && lost == 0) {
        alertLabel->setText(TR("Everything is on time — no overdue or missing books."));
        alertLabel->setObjectName("hintBox");
    } else {
        alertLabel->setText(QString(TR("%1 loan(s) overdue · %2 not returned after %3 days · "
                                       "total fines %4"))
                                .arg(overdue).arg(lost).arg(system->lostAfterDays())
                                .arg(LibrarySystem::formatMoney(fines)));
        alertLabel->setObjectName("warnBox");
    }
    // Re-applying the sheet makes Qt notice the changed object name.
    alertLabel->setStyleSheet(alertLabel->styleSheet());
    alertLabel->style()->unpolish(alertLabel);
    alertLabel->style()->polish(alertLabel);

    const QDate today = QDate::currentDate();
    activeTable->setSortingEnabled(false);
    activeTable->setRowCount(0);
    const QList<BorrowRecord> records = system->getAllBorrowRecords();
    const int filter = loanFilter->currentIndex(); // 0 all, 1 overdue, 2 not returned
    for (const BorrowRecord &r : records) {
        if (r.getStatus())
            continue; // only books still out
        if (filter == 1 && !r.isOverdue(today))
            continue;
        if (filter == 2 && !r.isLost(today, system->lostAfterDays()))
            continue;
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
        activeTable->setItem(row, 5, new QTableWidgetItem(
            r.getDueDate().isEmpty() ? QStringLiteral("—") : r.getDueDate()));

        const int late = r.lateDays(today);
        const bool lostItem = r.isLost(today, system->lostAfterDays());

        QString state = TR("On time");
        QColor colour = Theme::success();
        if (lostItem) {
            state = QString(TR("NOT RETURNED — %1 day(s) late")).arg(late);
            colour = Theme::critical();
        } else if (late > 0) {
            state = QString(TR("OVERDUE by %1 day(s)")).arg(late);
            colour = Theme::danger();
        }

        auto *statusItem = new QTableWidgetItem(state);
        statusItem->setForeground(QBrush(colour));
        activeTable->setItem(row, 6, statusItem);

        auto *fineItem = new QTableWidgetItem(
            LibrarySystem::formatMoney(r.fine(system->finePerDay(), today)));
        if (late > 0)
            fineItem->setForeground(QBrush(Theme::danger()));
        activeTable->setItem(row, 7, fineItem);
    }
    activeTable->setSortingEnabled(true);
}
