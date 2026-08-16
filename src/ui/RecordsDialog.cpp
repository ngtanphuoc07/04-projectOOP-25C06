#include "ui/RecordsDialog.h"

#include <QComboBox>
#include <QDate>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QVBoxLayout>

#include "i18n/Lang.h"
#include "ui/Exporter.h"
#include "ui/Theme.h"

RecordsDialog::RecordsDialog(LibrarySystem *system, const QString &title,
                             const QString &subtitle, const QList<BorrowRecord> &records,
                             QWidget *parent)
    : QDialog(parent), system(system), allRecords(records)
{
    setWindowTitle(title);
    resize(940, 500);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto *heading = new QLabel(subtitle, this);
    heading->setObjectName("dialogHeading");
    heading->setWordWrap(true);
    layout->addWidget(heading);

    auto *filterRow = new QHBoxLayout();
    filterRow->addWidget(new QLabel(TR("Filter:"), this));
    statusFilter = new QComboBox(this);
    statusFilter->addItems({TR("All records"), TR("Still borrowed"),
                            TR("Returned"), TR("Overdue")});
    filterRow->addWidget(statusFilter);
    filterRow->addStretch(1);
    summaryLabel = new QLabel(this);
    summaryLabel->setObjectName("dialogSummary");
    filterRow->addWidget(summaryLabel);
    layout->addLayout(filterRow);

    table = new QTableWidget(0, 8, this);
    table->setHorizontalHeaderLabels({TR("Record"), TR("Reader"), TR("Book"), TR("Title"),
                                      TR("Borrowed on"), TR("Due"), TR("Status"), TR("Fine")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    table->setSortingEnabled(true);
    layout->addWidget(table, 1);

    auto *exportButton = new QPushButton(TR("Export to CSV"), this);
    exportButton->setObjectName("secondaryButton");
    connect(exportButton, &QPushButton::clicked, this, [this] {
        Exporter::exportTableToCsv(table, "borrow-history", this);
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    buttons->addButton(exportButton, QDialogButtonBox::ActionRole);
    buttons->button(QDialogButtonBox::Close)->setText(TR("Close"));
    layout->addWidget(buttons);

    setStyleSheet(Theme::dialogStyleSheet());

    connect(statusFilter, &QComboBox::currentIndexChanged, this, [this](int) { fillTable(); });
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    fillTable();
}

void RecordsDialog::fillTable()
{
    const int filter = statusFilter->currentIndex(); // 0 all, 1 out, 2 returned, 3 overdue
    const QDate today = QDate::currentDate();
    const int perDay = system->finePerDay();
    const int lostAfter = system->lostAfterDays();

    // Sorting has to be off while rows are inserted, otherwise the table
    // reorders itself between insertRow() and setItem().
    table->setSortingEnabled(false);
    table->setRowCount(0);

    int outCount = 0;
    int overdueCount = 0;

    for (const BorrowRecord &r : allRecords) {
        if (!r.getStatus())
            ++outCount;
        const bool overdue = r.isOverdue(today);
        if (overdue)
            ++overdueCount;

        if (filter == 1 && r.getStatus())
            continue;
        if (filter == 2 && !r.getStatus())
            continue;
        if (filter == 3 && !overdue)
            continue;

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

        const int late = r.lateDays(today);
        QString state;
        QColor colour;
        if (r.getStatus() && late > 0) {
            // Say plainly that it came back late, and what it cost.
            state = QString(TR("Returned late — %1 day(s), fine %2"))
                        .arg(late).arg(LibrarySystem::formatMoney(r.fine(perDay, today)));
            colour = Theme::danger();
        } else if (r.getStatus()) {
            state = QString("%1 %2").arg(TR("Returned"), r.getReturnDate());
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
        table->setItem(row, 6, statusItem);

        auto *fineItem = new QTableWidgetItem(LibrarySystem::formatMoney(r.fine(perDay, today)));
        if (late > 0)
            fineItem->setForeground(QBrush(Theme::danger()));
        table->setItem(row, 7, fineItem);
    }

    table->setSortingEnabled(true);

    summaryLabel->setText(QString(TR("%1 record(s) · %2 still out · %3 overdue"))
                              .arg(allRecords.size())
                              .arg(outCount)
                              .arg(overdueCount));
}
