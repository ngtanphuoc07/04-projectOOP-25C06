#include "i18n/Lang.h"
#include "ui/ReadersPage.h"

#include <QDate>
#include <QSet>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QVBoxLayout>

#include "ui/ReaderDetailDialog.h"
#include "ui/RecordsDialog.h"
#include "ui/SuggestBox.h"
#include "ui/Theme.h"

ReadersPage::ReadersPage(LibrarySystem *system, QWidget *parent)
    : QWidget(parent), system(system)
{
    auto *layout = new QHBoxLayout(this);
    setObjectName("pageBody");
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(14);

    // ------------------------------------------------ left: search + table
    auto *leftLayout = new QVBoxLayout();

    auto *searchLayout = new QHBoxLayout();
    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText(TR("Search by name, ID, phone or email..."));
    searchEdit->setClearButtonEnabled(true);
    SuggestBox::attach(searchEdit, [this] {
        QStringList out;
        const QList<Reader> all = this->system->readerManager()->getAllReaders();
        for (const Reader &r : all)
            out << r.getFullName() << r.getReaderID() << r.getEmail() << r.getPhone();
        return out;
    });
    auto *searchButton = new QPushButton(TR("Search"), this);
    auto *showAllButton = new QPushButton(TR("Show all"), this);
    showAllButton->setObjectName("secondaryButton");
    searchLayout->addWidget(searchEdit, 1);
    searchLayout->addWidget(searchButton);
    searchLayout->addWidget(showAllButton);
    leftLayout->addLayout(searchLayout);

    table = new QTableWidget(0, 5, this);
    table->setHorizontalHeaderLabels({TR("ID"), TR("Full name"), TR("Phone"), TR("Email"), TR("Borrowed")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    table->setSortingEnabled(true); // click a header to sort
    table->setMinimumHeight(320);
    leftLayout->addWidget(table, 1);

    detailLabel = new QLabel(TR("Select a reader to see their details."), this);
    detailLabel->setObjectName("detailBox");
    detailLabel->setMinimumHeight(64);
    detailLabel->setMaximumHeight(96);
    leftLayout->addWidget(detailLabel);

    auto *actionRow = new QHBoxLayout();
    detailButton = new QPushButton(TR("Open full details"), this);
    detailButton->setEnabled(false);
    actionRow->addWidget(detailButton);

    loansButton = new QPushButton(TR("View books borrowed by selected reader"), this);
    loansButton->setObjectName("secondaryButton");
    loansButton->setEnabled(false);
    actionRow->addWidget(loansButton);
    actionRow->addStretch(1);
    leftLayout->addLayout(actionRow);

    layout->addLayout(leftLayout, 2);

    // ----------------------------------------------------- right: the form
    auto *formGroup = new QGroupBox(TR("Reader details"), this);
    formGroup->setMinimumWidth(320);
    formGroup->setMaximumWidth(380);
    auto *formOuter = new QVBoxLayout(formGroup);
    auto *form = new QFormLayout();

    idEdit = new QLineEdit(formGroup);
    idEdit->setPlaceholderText(TR("filled in automatically"));
    nameEdit = new QLineEdit(formGroup);
    phoneEdit = new QLineEdit(formGroup);
    phoneEdit->setPlaceholderText(TR("e.g. 0901234567 or +84 901 234 567"));
    emailEdit = new QLineEdit(formGroup);
    emailEdit->setPlaceholderText(TR("name@example.com"));

    form->addRow(TR("Reader ID:"), idEdit);
    form->addRow(TR("Full name:"), nameEdit);
    form->addRow(TR("Phone:"), phoneEdit);
    form->addRow(TR("Email:"), emailEdit);
    formOuter->addLayout(form);

    addButton = new QPushButton(TR("Add reader"), formGroup);
    updateButton = new QPushButton(TR("Update selected"), formGroup);
    deleteButton = new QPushButton(TR("Delete selected"), formGroup);
    deleteButton->setObjectName("dangerButton");
    auto *clearButton = new QPushButton(TR("Clear form"), formGroup);
    clearButton->setObjectName("secondaryButton");
    updateButton->setObjectName("secondaryButton");
    updateButton->setEnabled(false);
    deleteButton->setEnabled(false);

    formOuter->addWidget(addButton);
    formOuter->addWidget(updateButton);
    formOuter->addWidget(deleteButton);
    formOuter->addWidget(clearButton);
    formOuter->addStretch(1);

    layout->addWidget(formGroup, 1);

    connect(searchButton, &QPushButton::clicked, this, &ReadersPage::onSearch);
    connect(searchEdit, &QLineEdit::returnPressed, this, &ReadersPage::onSearch);
    connect(showAllButton, &QPushButton::clicked, this, [this] {
        searchEdit->clear();
        refresh();
    });
    connect(table, &QTableWidget::itemSelectionChanged, this, &ReadersPage::onRowSelected);
    connect(addButton, &QPushButton::clicked, this, &ReadersPage::onAdd);
    connect(updateButton, &QPushButton::clicked, this, &ReadersPage::onUpdate);
    connect(deleteButton, &QPushButton::clicked, this, &ReadersPage::onDelete);
    connect(clearButton, &QPushButton::clicked, this, &ReadersPage::onClearForm);
    connect(loansButton, &QPushButton::clicked, this, &ReadersPage::onShowLoans);
    connect(detailButton, &QPushButton::clicked, this, &ReadersPage::onShowDetail);
    // Double-clicking a row is the shortcut most people reach for first.
    connect(table, &QTableWidget::itemDoubleClicked, this, &ReadersPage::onShowDetail);
    connect(system, &LibrarySystem::dataChanged, this, &ReadersPage::refresh);

    refresh();
    onClearForm();
}

void ReadersPage::refresh()
{
    const QString keyword = searchEdit->text().trimmed();
    if (keyword.isEmpty())
        fillTable(system->readerManager()->getAllReaders());
    else
        fillTable(system->readerManager()->searchReaders(keyword));
}

void ReadersPage::onSearch()
{
    refresh();
}

void ReadersPage::fillTable(const QList<Reader> &readers)
{
    shownReaders = readers;

    table->setSortingEnabled(false);
    table->setRowCount(0);

    // Computed once for the whole table instead of once per row: the old
    // version ran a full query for every reader, which was quadratic.
    const QSet<QString> lateReaders = system->readersWithOverdue();

    for (int i = 0; i < readers.size(); ++i) {
        const Reader &r = readers.at(i);
        const int row = table->rowCount();
        table->insertRow(row);

        auto *idItem = new QTableWidgetItem(r.getReaderID());
        // Survives the user sorting the table by any column.
        idItem->setData(Qt::UserRole, i);
        table->setItem(row, 0, idItem);

        table->setItem(row, 1, new QTableWidgetItem(r.getFullName()));
        table->setItem(row, 2, new QTableWidgetItem(r.getPhone()));
        table->setItem(row, 3, new QTableWidgetItem(r.getEmail()));

        auto *countItem = new QTableWidgetItem();
        countItem->setData(Qt::DisplayRole, r.getBorrowedCount());

        // Anyone holding a late book is flagged straight in the list, so the
        // librarian does not have to open each reader to find out.
        if (lateReaders.contains(r.getReaderID())) {
            countItem->setForeground(QBrush(Theme::danger()));
            countItem->setToolTip(TR("This reader is holding at least one overdue book."));
        }
        table->setItem(row, 4, countItem);
    }

    table->setSortingEnabled(true);
}

int ReadersPage::selectedIndex() const
{
    const int row = table->currentRow();
    if (row < 0)
        return -1;
    const QTableWidgetItem *idItem = table->item(row, 0);
    if (!idItem)
        return -1;
    const int index = idItem->data(Qt::UserRole).toInt();
    return (index >= 0 && index < shownReaders.size()) ? index : -1;
}

void ReadersPage::onRowSelected()
{
    const int index = selectedIndex();
    if (index < 0) {
        updateButton->setEnabled(false);
        deleteButton->setEnabled(false);
        loansButton->setEnabled(false);
        detailButton->setEnabled(false);
        return;
    }
    // A value copy, not a reference: the facade emits dataChanged, which
    // refills shownReaders and would leave a reference dangling.
    const Reader r = shownReaders.at(index);
    idEdit->setText(r.getReaderID());
    idEdit->setReadOnly(true);
    nameEdit->setText(r.getFullName());
    phoneEdit->setText(r.getPhone());
    emailEdit->setText(r.getEmail());
    updateButton->setEnabled(true);
    deleteButton->setEnabled(true);
    loansButton->setEnabled(true);
    detailButton->setEnabled(true);
    showDetails(r);
}

void ReadersPage::showDetails(const Entity &entity)
{
    detailLabel->setText(entity.displayInfo());
}

void ReadersPage::onShowDetail()
{
    const int index = selectedIndex();
    if (index < 0)
        return;
    // A librarian may also set the reader's photo from here.
    ReaderDetailDialog dialog(system, shownReaders.at(index).getReaderID(), true, this);
    dialog.exec();
}

void ReadersPage::onShowLoans()
{
    const int index = selectedIndex();
    if (index < 0)
        return;
    // A value copy, not a reference: the facade emits dataChanged, which
    // refills shownReaders and would leave a reference dangling.
    const Reader r = shownReaders.at(index);

    const QList<BorrowRecord> active = system->getActiveRecordsByReader(r.getReaderID());
    RecordsDialog dialog(
        system,
        QString(TR("Books borrowed by %1")).arg(r.getFullName()),
        QString(TR("%1 — %2\nHolding %3 of a maximum of %4 books. "
                   "Use the filter to switch between current loans and the full history."))
            .arg(r.getReaderID(), r.getFullName())
            .arg(active.size())
            .arg(system->maxBooksPerReader()),
        system->getRecordsByReader(r.getReaderID()), this);
    dialog.exec();
}

bool ReadersPage::validateForm(QString *errorOut)
{
    if (idEdit->text().trimmed().isEmpty()) {
        *errorOut = TR("Reader ID must not be empty.");
        return false;
    }
    if (nameEdit->text().trimmed().isEmpty()) {
        *errorOut = TR("Full name must not be empty.");
        return false;
    }
    // Spaces, dashes and a +84 prefix are stripped rather than rejected, then
    // the digits that remain are checked.
    const QString phone = ReaderManager::normalisePhone(phoneEdit->text());
    if (phone.length() < 9 || phone.length() > 11) {
        *errorOut = TR("Phone must contain 9–11 digits.");
        return false;
    }
    phoneEdit->setText(phone); // show the user what will be stored
    static const QRegularExpression emailPattern("^[\\w.+-]+@[\\w-]+(\\.[\\w-]+)+$");
    if (!emailPattern.match(emailEdit->text().trimmed()).hasMatch()) {
        *errorOut = TR("Email address is not valid.");
        return false;
    }
    return true;
}

void ReadersPage::onAdd()
{
    QString error;
    if (!validateForm(&error)) {
        QMessageBox::warning(this, TR("Invalid input"), error);
        return;
    }
    Reader reader(idEdit->text().trimmed(), nameEdit->text().trimmed(),
                  phoneEdit->text().trimmed(), emailEdit->text().trimmed(), 0);
    if (!system->readerManager()->addReader(reader, &error)) {
        QMessageBox::warning(this, TR("Cannot add reader"), error);
        return;
    }
    system->notifyDataChanged();
    onClearForm();
}

void ReadersPage::onUpdate()
{
    QString error;
    if (!validateForm(&error)) {
        QMessageBox::warning(this, TR("Invalid input"), error);
        return;
    }
    Reader reader(idEdit->text().trimmed(), nameEdit->text().trimmed(),
                  phoneEdit->text().trimmed(), emailEdit->text().trimmed(), 0);
    if (!system->readerManager()->updateReader(reader, &error)) {
        QMessageBox::warning(this, TR("Cannot update reader"), error);
        return;
    }
    system->notifyDataChanged();
    onClearForm();
}

void ReadersPage::onDelete()
{
    const QString readerID = idEdit->text().trimmed();
    if (readerID.isEmpty())
        return;
    const auto answer = QMessageBox::question(
        this, TR("Delete reader"),
        QString(TR("Really delete reader \"%1\"?\nThis cannot be undone.")).arg(readerID));
    if (answer != QMessageBox::Yes)
        return;

    QString error;
    if (!system->deleteReaderAndAccount(readerID, &error)) {
        QMessageBox::warning(this, TR("Cannot delete reader"), error);
        return;
    }
    system->notifyDataChanged();
    onClearForm();
}

void ReadersPage::onClearForm()
{
    table->clearSelection();
    idEdit->setText(system->readerManager()->nextReaderID());
    idEdit->setReadOnly(false);
    nameEdit->clear();
    phoneEdit->clear();
    emailEdit->clear();
    updateButton->setEnabled(false);
    deleteButton->setEnabled(false);
    loansButton->setEnabled(false);
    detailButton->setEnabled(false);
    detailLabel->setText(TR("Select a reader to see their details."));
}
