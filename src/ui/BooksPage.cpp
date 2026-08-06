#include "ui/BooksPage.h"

#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

BooksPage::BooksPage(LibrarySystem *system, QWidget *parent)
    : QWidget(parent), system(system)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(16, 16, 16, 16);
    layout->setSpacing(14);

    // ------------------------------------------------ left: search + table
    auto *leftLayout = new QVBoxLayout();

    auto *searchLayout = new QHBoxLayout();
    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText("Search books by title...");
    auto *searchButton = new QPushButton("Search", this);
    auto *showAllButton = new QPushButton("Show all", this);
    searchLayout->addWidget(searchEdit, 1);
    searchLayout->addWidget(searchButton);
    searchLayout->addWidget(showAllButton);
    leftLayout->addLayout(searchLayout);

    table = new QTableWidget(0, 6, this);
    table->setHorizontalHeaderLabels({"ID", "Title", "Author", "Category", "Total", "Available"});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->setVisible(false);
    leftLayout->addWidget(table, 1);

    detailLabel = new QLabel("Select a book to see its details.", this);
    detailLabel->setObjectName("detailBox");
    detailLabel->setMinimumHeight(64);
    leftLayout->addWidget(detailLabel);

    layout->addLayout(leftLayout, 2);

    // ----------------------------------------------------- right: the form
    auto *formGroup = new QGroupBox("Book details", this);
    formGroup->setMaximumWidth(340);
    auto *formOuter = new QVBoxLayout(formGroup);
    auto *form = new QFormLayout();

    idEdit = new QLineEdit(formGroup);
    idEdit->setPlaceholderText("e.g. B006");
    titleEdit = new QLineEdit(formGroup);
    authorEdit = new QLineEdit(formGroup);
    categoryEdit = new QLineEdit(formGroup);
    totalSpin = new QSpinBox(formGroup);
    totalSpin->setRange(1, 9999);

    form->addRow("Book ID:", idEdit);
    form->addRow("Title:", titleEdit);
    form->addRow("Author:", authorEdit);
    form->addRow("Category:", categoryEdit);
    form->addRow("Total quantity:", totalSpin);
    formOuter->addLayout(form);

    addButton = new QPushButton("Add book", formGroup);
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

    connect(searchButton, &QPushButton::clicked, this, &BooksPage::onSearch);
    connect(searchEdit, &QLineEdit::returnPressed, this, &BooksPage::onSearch);
    connect(showAllButton, &QPushButton::clicked, this, [this] {
        searchEdit->clear();
        refresh();
    });
    connect(table, &QTableWidget::itemSelectionChanged, this, &BooksPage::onRowSelected);
    connect(addButton, &QPushButton::clicked, this, &BooksPage::onAdd);
    connect(updateButton, &QPushButton::clicked, this, &BooksPage::onUpdate);
    connect(deleteButton, &QPushButton::clicked, this, &BooksPage::onDelete);
    connect(clearButton, &QPushButton::clicked, this, &BooksPage::onClearForm);
    connect(system, &LibrarySystem::dataChanged, this, &BooksPage::refresh);

    refresh();
}

void BooksPage::refresh()
{
    const QString keyword = searchEdit->text().trimmed();
    if (keyword.isEmpty())
        fillTable(system->bookManager()->getAllBooks());
    else
        fillTable(system->bookManager()->searchBooksByTitle(keyword));
}

void BooksPage::onSearch()
{
    refresh();
}

void BooksPage::fillTable(const QList<Book> &books)
{
    shownBooks = books;
    table->setRowCount(0);
    for (const Book &b : books) {
        const int row = table->rowCount();
        table->insertRow(row);
        table->setItem(row, 0, new QTableWidgetItem(b.getBookID()));
        table->setItem(row, 1, new QTableWidgetItem(b.getTitle()));
        table->setItem(row, 2, new QTableWidgetItem(b.getAuthor()));
        table->setItem(row, 3, new QTableWidgetItem(b.getCategory()));
        table->setItem(row, 4, new QTableWidgetItem(QString::number(b.getTotalQuantity())));
        table->setItem(row, 5, new QTableWidgetItem(QString::number(b.getAvailableQuantity())));
    }
}

void BooksPage::onRowSelected()
{
    const int row = table->currentRow();
    if (row < 0 || row >= shownBooks.size()) {
        updateButton->setEnabled(false);
        deleteButton->setEnabled(false);
        return;
    }
    const Book &b = shownBooks.at(row);
    idEdit->setText(b.getBookID());
    idEdit->setReadOnly(true);
    titleEdit->setText(b.getTitle());
    authorEdit->setText(b.getAuthor());
    categoryEdit->setText(b.getCategory());
    totalSpin->setValue(b.getTotalQuantity());
    updateButton->setEnabled(true);
    deleteButton->setEnabled(true);
    showDetails(b);
}

void BooksPage::showDetails(const Entity &entity)
{
    // Runtime polymorphism: 'entity' is a Book here, but the same call works
    // for Reader and BorrowRecord on their pages.
    detailLabel->setText(entity.displayInfo());
}

bool BooksPage::validateForm(QString *errorOut)
{
    if (idEdit->text().trimmed().isEmpty()) {
        *errorOut = "Book ID must not be empty.";
        return false;
    }
    if (titleEdit->text().trimmed().isEmpty()) {
        *errorOut = "Title must not be empty.";
        return false;
    }
    if (authorEdit->text().trimmed().isEmpty()) {
        *errorOut = "Author must not be empty.";
        return false;
    }
    return true;
}

void BooksPage::onAdd()
{
    QString error;
    if (!validateForm(&error)) {
        QMessageBox::warning(this, "Invalid input", error);
        return;
    }
    const int total = totalSpin->value();
    Book book(idEdit->text().trimmed(), titleEdit->text().trimmed(),
              authorEdit->text().trimmed(), categoryEdit->text().trimmed(),
              total, total);
    if (!system->bookManager()->addBook(book, &error)) {
        QMessageBox::warning(this, "Cannot add book", error);
        return;
    }
    system->notifyDataChanged();
    onClearForm();
}

void BooksPage::onUpdate()
{
    QString error;
    if (!validateForm(&error)) {
        QMessageBox::warning(this, "Invalid input", error);
        return;
    }
    Book book(idEdit->text().trimmed(), titleEdit->text().trimmed(),
              authorEdit->text().trimmed(), categoryEdit->text().trimmed(),
              totalSpin->value(), 0); // available is recomputed by BookManager
    if (!system->bookManager()->updateBook(book, &error)) {
        QMessageBox::warning(this, "Cannot update book", error);
        return;
    }
    system->notifyDataChanged();
    onClearForm();
}

void BooksPage::onDelete()
{
    const QString bookID = idEdit->text().trimmed();
    if (bookID.isEmpty())
        return;
    const auto answer = QMessageBox::question(
        this, "Delete book",
        QString("Really delete book \"%1\"?").arg(bookID));
    if (answer != QMessageBox::Yes)
        return;

    QString error;
    if (!system->bookManager()->deleteBook(bookID, &error)) {
        QMessageBox::warning(this, "Cannot delete book", error);
        return;
    }
    system->notifyDataChanged();
    onClearForm();
}

void BooksPage::onClearForm()
{
    table->clearSelection();
    idEdit->clear();
    idEdit->setReadOnly(false);
    titleEdit->clear();
    authorEdit->clear();
    categoryEdit->clear();
    totalSpin->setValue(1);
    updateButton->setEnabled(false);
    deleteButton->setEnabled(false);
    detailLabel->setText("Select a book to see its details.");
}
