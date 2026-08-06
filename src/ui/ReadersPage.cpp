#include "ui/ReadersPage.h"

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

ReadersPage::ReadersPage(LibrarySystem *system, QWidget *parent)
    : QWidget(parent), system(system)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    // ------------------------------------------------ left: search + table
    auto *leftLayout = new QVBoxLayout();

    auto *searchLayout = new QHBoxLayout();
    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText("Search readers by name...");
    auto *searchButton = new QPushButton("Search", this);
    auto *showAllButton = new QPushButton("Show all", this);
    searchLayout->addWidget(searchEdit, 1);
    searchLayout->addWidget(searchButton);
    searchLayout->addWidget(showAllButton);
    leftLayout->addLayout(searchLayout);

    table = new QTableWidget(0, 5, this);
    table->setHorizontalHeaderLabels({"ID", "Full name", "Phone", "Email", "Borrowed"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->setVisible(false);
    leftLayout->addWidget(table, 1);

    detailLabel = new QLabel("Select a reader to see their details.", this);
    detailLabel->setObjectName("detailBox");
    detailLabel->setMinimumHeight(64);
    leftLayout->addWidget(detailLabel);

    layout->addLayout(leftLayout, 2);

    // ----------------------------------------------------- right: the form
    auto *formGroup = new QGroupBox("Reader details", this);
    formGroup->setMaximumWidth(340);
    auto *formOuter = new QVBoxLayout(formGroup);
    auto *form = new QFormLayout();

    idEdit = new QLineEdit(formGroup);
    idEdit->setPlaceholderText("e.g. R004");
    nameEdit = new QLineEdit(formGroup);
    phoneEdit = new QLineEdit(formGroup);
    phoneEdit->setPlaceholderText("digits only, e.g. 0901234567");
    emailEdit = new QLineEdit(formGroup);
    emailEdit->setPlaceholderText("name@example.com");

    form->addRow("Reader ID:", idEdit);
    form->addRow("Full name:", nameEdit);
    form->addRow("Phone:", phoneEdit);
    form->addRow("Email:", emailEdit);
    formOuter->addLayout(form);

    addButton = new QPushButton("Add reader", formGroup);
    updateButton = new QPushButton("Update selected", formGroup);
    deleteButton = new QPushButton("Delete selected", formGroup);
    deleteButton->setObjectName("dangerButton");
    auto *clearButton = new QPushButton("Clear form", formGroup);
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
    connect(system, &LibrarySystem::dataChanged, this, &ReadersPage::refresh);

    refresh();
}

void ReadersPage::refresh()
{
    const QString keyword = searchEdit->text().trimmed();
    if (keyword.isEmpty())
        fillTable(system->readerManager()->getAllReaders());
    else
        fillTable(system->readerManager()->searchReadersByName(keyword));
}

void ReadersPage::onSearch()
{
    refresh();
}

void ReadersPage::fillTable(const QList<Reader> &readers)
{
    shownReaders = readers;
    table->setRowCount(0);
    for (const Reader &r : readers) {
        const int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(r.getReaderID()));
        table->setItem(row, 1, new QTableWidgetItem(r.getFullName()));
        table->setItem(row, 2, new QTableWidgetItem(r.getPhone()));
        table->setItem(row, 3, new QTableWidgetItem(r.getEmail()));
        table->setItem(row, 4, new QTableWidgetItem(QString::number(r.getBorrowedCount())));
    }
}

void ReadersPage::onRowSelected()
{
    const int row = table->currentRow();
    if (row < 0 || row >= shownReaders.size()) {
        updateButton->setEnabled(false);
        deleteButton->setEnabled(false);
        return;
    }
    const Reader &r = shownReaders.at(row);
    idEdit->setText(r.getReaderID());
    idEdit->setReadOnly(true);
    nameEdit->setText(r.getFullName());
    phoneEdit->setText(r.getPhone());
    emailEdit->setText(r.getEmail());
    updateButton->setEnabled(true);
    deleteButton->setEnabled(true);
    showDetails(r);
}

void ReadersPage::showDetails(const Entity &entity)
{
    detailLabel->setText(entity.displayInfo());
}

bool ReadersPage::validateForm(QString *errorOut)
{
    if (idEdit->text().trimmed().isEmpty()) {
        *errorOut = "Reader ID must not be empty.";
        return false;
    }
    if (nameEdit->text().trimmed().isEmpty()) {
        *errorOut = "Full name must not be empty.";
        return false;
    }
    static const QRegularExpression phonePattern("^\\d{9,11}$");
    if (!phonePattern.match(phoneEdit->text().trimmed()).hasMatch()) {
        *errorOut = "Phone must be 9–11 digits.";
        return false;
    }
    static const QRegularExpression emailPattern("^[\\w.+-]+@[\\w-]+(\\.[\\w-]+)+$");
    if (!emailPattern.match(emailEdit->text().trimmed()).hasMatch()) {
        *errorOut = "Email address is not valid.";
        return false;
    }
    return true;
}

void ReadersPage::onAdd()
{
    QString error;
    if (!validateForm(&error)) {
        QMessageBox::warning(this, "Invalid input", error);
        return;
    }
    Reader reader(idEdit->text().trimmed(), nameEdit->text().trimmed(),
                  phoneEdit->text().trimmed(), emailEdit->text().trimmed(), 0);
    if (!system->readerManager()->addReader(reader, &error)) {
        QMessageBox::warning(this, "Cannot add reader", error);
        return;
    }
    system->notifyDataChanged();
    onClearForm();
}

void ReadersPage::onUpdate()
{
    QString error;
    if (!validateForm(&error)) {
        QMessageBox::warning(this, "Invalid input", error);
        return;
    }
    Reader reader(idEdit->text().trimmed(), nameEdit->text().trimmed(),
                  phoneEdit->text().trimmed(), emailEdit->text().trimmed(), 0);
    if (!system->readerManager()->updateReader(reader, &error)) {
        QMessageBox::warning(this, "Cannot update reader", error);
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
        this, "Delete reader",
        QString("Really delete reader \"%1\"?").arg(readerID));
    if (answer != QMessageBox::Yes)
        return;

    QString error;
    if (!system->readerManager()->deleteReader(readerID, &error)) {
        QMessageBox::warning(this, "Cannot delete reader", error);
        return;
    }
    system->notifyDataChanged();
    onClearForm();
}

void ReadersPage::onClearForm()
{
    table->clearSelection();
    idEdit->clear();
    idEdit->setReadOnly(false);
    nameEdit->clear();
    phoneEdit->clear();
    emailEdit->clear();
    updateButton->setEnabled(false);
    deleteButton->setEnabled(false);
    detailLabel->setText("Select a reader to see their details.");
}
