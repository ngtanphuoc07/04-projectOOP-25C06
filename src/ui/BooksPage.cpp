#include "ui/BooksPage.h"

#include <QDate>
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

#include "i18n/Lang.h"
#include "ui/RecordsDialog.h"
#include "ui/SuggestBox.h"
#include "ui/Theme.h"

BooksPage::BooksPage(LibrarySystem *system, bool editable, const QString &selfReaderID,
                     QWidget *parent)
    : QWidget(parent), system(system), editable(editable), selfReaderID(selfReaderID)
{
    setObjectName("pageBody");

    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(16);

    // ------------------------------------------------ left: search + table
    auto *leftLayout = new QVBoxLayout();
    leftLayout->setSpacing(10);

    auto *searchLayout = new QHBoxLayout();
    searchEdit = new QLineEdit(this);
    searchEdit->setPlaceholderText(TR("Search by title, author, category or ID..."));
    searchEdit->setClearButtonEnabled(true);
    // Drop-down suggestions as you type, like a search engine.
    SuggestBox::attach(searchEdit, [this] {
        QStringList out;
        const QList<Book> all = this->system->bookManager()->getAllBooks();
        for (const Book &b : all)
            out << b.getTitle() << b.getAuthor() << b.getCategory();
        return out;
    });
    auto *searchButton = new QPushButton(TR("Search"), this);
    auto *showAllButton = new QPushButton(TR("Show all"), this);
    showAllButton->setObjectName("secondaryButton");
    searchLayout->addWidget(searchEdit, 1);
    searchLayout->addWidget(searchButton);
    searchLayout->addWidget(showAllButton);
    leftLayout->addLayout(searchLayout);

    table = new QTableWidget(0, 6, this);
    table->setHorizontalHeaderLabels({TR("ID"), TR("Title"), TR("Author"), TR("Category"),
                                      TR("Total"), TR("Available")});
    table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setVisible(false);
    table->setSortingEnabled(true); // click a header to sort
    // Without a floor the table collapses to a couple of rows inside the
    // scroll area, which is what made this screen unusable.
    table->setMinimumHeight(320);
    leftLayout->addWidget(table, 1);

    detailLabel = new QLabel(TR("Select a book to see its details."), this);
    detailLabel->setObjectName("detailBox");
    detailLabel->setMinimumHeight(68);
    detailLabel->setMaximumHeight(140);  // room for the borrower list
    detailLabel->setWordWrap(true);
    leftLayout->addWidget(detailLabel);

    auto *actionRow = new QHBoxLayout();

    // A patron never creates a loan themselves: they place a request and the
    // librarian hands the book over at the desk. The button exists only so the
    // old code path keeps compiling; it is never shown.
    borrowButton = new QPushButton(TR("Borrow this book"), this);
    borrowButton->setEnabled(false);
    borrowButton->setVisible(false);
    actionRow->addWidget(borrowButton);

    // The patron's only way to get a book: request it, then collect it.
    reserveButton = new QPushButton(TR("Request this book"), this);
    reserveButton->setObjectName("secondaryButton");
    reserveButton->setEnabled(false);
    reserveButton->setVisible(!selfReaderID.isEmpty());
    actionRow->addWidget(reserveButton);

    historyButton = new QPushButton(TR("View borrow history of selected book"), this);
    historyButton->setObjectName("secondaryButton");
    historyButton->setEnabled(false);
    // A patron must never see who else borrowed a title, so the button that
    // opens that list simply does not exist for them.
    historyButton->setVisible(selfReaderID.isEmpty() && editable);
    actionRow->addWidget(historyButton);
    actionRow->addStretch(1);
    leftLayout->addLayout(actionRow);

    // Explains borrow limits / overdue blocks before the patron presses Borrow.
    noticeLabel = new QLabel(this);
    noticeLabel->setObjectName("warnBox");
    noticeLabel->setWordWrap(true);
    noticeLabel->setVisible(false);
    leftLayout->addWidget(noticeLabel);

    layout->addLayout(leftLayout, 2);

    // ----------------------------------------------------- right: the form
    formGroup = new QGroupBox(TR("Book details"), this);
    formGroup->setMinimumWidth(320);
    formGroup->setMaximumWidth(380);
    auto *formOuter = new QVBoxLayout(formGroup);
    formOuter->setSpacing(9);
    auto *form = new QFormLayout();
    form->setSpacing(9);

    idEdit = new QLineEdit(formGroup);
    idEdit->setPlaceholderText(TR("filled in automatically"));
    titleEdit = new QLineEdit(formGroup);
    authorEdit = new QLineEdit(formGroup);
    categoryEdit = new QLineEdit(formGroup);
    totalSpin = new QSpinBox(formGroup);
    totalSpin->setRange(1, 9999);

    form->addRow(TR("Book ID:"), idEdit);
    form->addRow(TR("Title:"), titleEdit);
    form->addRow(TR("Author:"), authorEdit);
    form->addRow(TR("Category:"), categoryEdit);
    form->addRow(TR("Total quantity:"), totalSpin);
    formOuter->addLayout(form);

    addButton = new QPushButton(TR("Add book"), formGroup);
    updateButton = new QPushButton(TR("Update selected"), formGroup);
    updateButton->setObjectName("secondaryButton");
    deleteButton = new QPushButton(TR("Delete selected"), formGroup);
    deleteButton->setObjectName("dangerButton");
    auto *clearButton = new QPushButton(TR("Clear form"), formGroup);
    clearButton->setObjectName("secondaryButton");
    updateButton->setEnabled(false);
    deleteButton->setEnabled(false);

    formOuter->addWidget(addButton);
    formOuter->addWidget(updateButton);
    formOuter->addWidget(deleteButton);
    formOuter->addWidget(clearButton);
    formOuter->addStretch(1);

    layout->addWidget(formGroup, 1);
    formGroup->setVisible(editable); // readers browse, they do not edit

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
    connect(historyButton, &QPushButton::clicked, this, &BooksPage::onShowHistory);
    connect(borrowButton, &QPushButton::clicked, this, &BooksPage::onBorrowSelected);
    connect(reserveButton, &QPushButton::clicked, this, &BooksPage::onReserveSelected);
    connect(system, &LibrarySystem::dataChanged, this, &BooksPage::refresh);

    refresh();
    onClearForm();
}

void BooksPage::refresh()
{
    const QString keyword = searchEdit->text().trimmed();
    if (keyword.isEmpty())
        fillTable(system->bookManager()->getAllBooks());
    else
        fillTable(system->bookManager()->searchBooks(keyword));

    // Patron mode: say up front why borrowing may be blocked.
    if (!selfReaderID.isEmpty()) {
        const QList<BorrowRecord> late = system->getOverdueRecordsByReader(selfReaderID);
        const int holding = system->getActiveRecordsByReader(selfReaderID).size();
        if (!late.isEmpty()) {
            noticeLabel->setText(TR("You have an overdue book. Please return it before borrowing again."));
            noticeLabel->setVisible(true);
        } else if (holding >= system->maxBooksPerReader()) {
            noticeLabel->setText(QString(TR("You are holding %1 book(s); the limit is %2."))
                                     .arg(holding).arg(system->maxBooksPerReader()));
            noticeLabel->setVisible(true);
        } else {
            noticeLabel->setVisible(false);
        }
    }
}

void BooksPage::onSearch()
{
    refresh();
}

void BooksPage::fillTable(const QList<Book> &books)
{
    shownBooks = books;

    // Sorting must be off while the rows are built, otherwise the table
    // reshuffles itself between insertRow() and setItem().
    table->setSortingEnabled(false);
    table->setRowCount(0);

    for (int i = 0; i < books.size(); ++i) {
        const Book &b = books.at(i);
        const int row = table->rowCount();
        table->insertRow(row);

        auto *idItem = new QTableWidgetItem(b.getBookID());
        // The row order changes when the user sorts, so remember which entry of
        // shownBooks this row came from instead of trusting the row number.
        idItem->setData(Qt::UserRole, i);
        table->setItem(row, 0, idItem);

        table->setItem(row, 1, new QTableWidgetItem(b.getTitle()));
        table->setItem(row, 2, new QTableWidgetItem(b.getAuthor()));
        table->setItem(row, 3, new QTableWidgetItem(b.getCategory()));

        // Stored as numbers so "10" sorts after "9" instead of before it.
        auto *totalItem = new QTableWidgetItem();
        totalItem->setData(Qt::DisplayRole, b.getTotalQuantity());
        table->setItem(row, 4, totalItem);

        auto *availItem = new QTableWidgetItem();
        availItem->setData(Qt::DisplayRole, b.getAvailableQuantity());
        if (b.getAvailableQuantity() == 0)
            availItem->setForeground(QBrush(Theme::danger())); // nothing left on the shelf
        else
            availItem->setForeground(QBrush(Theme::success()));
        table->setItem(row, 5, availItem);
    }

    table->setSortingEnabled(true);
}

int BooksPage::selectedIndex() const
{
    const int row = table->currentRow();
    if (row < 0)
        return -1;
    const QTableWidgetItem *idItem = table->item(row, 0);
    if (!idItem)
        return -1;
    const int index = idItem->data(Qt::UserRole).toInt();
    return (index >= 0 && index < shownBooks.size()) ? index : -1;
}

void BooksPage::onRowSelected()
{
    const int index = selectedIndex();
    if (index < 0) {
        updateButton->setEnabled(false);
        deleteButton->setEnabled(false);
        historyButton->setEnabled(false);
        borrowButton->setEnabled(false);
        reserveButton->setEnabled(false);
        return;
    }
    // A value copy, not a reference: the facade emits dataChanged, which
    // refills shownBooks and would leave a reference dangling.
    const Book b = shownBooks.at(index);
    idEdit->setText(b.getBookID());
    idEdit->setReadOnly(true);
    titleEdit->setText(b.getTitle());
    authorEdit->setText(b.getAuthor());
    categoryEdit->setText(b.getCategory());
    totalSpin->setValue(b.getTotalQuantity());
    updateButton->setEnabled(editable);
    deleteButton->setEnabled(editable);
    historyButton->setEnabled(true);
    borrowButton->setEnabled(false);
    reserveButton->setEnabled(true);
    showDetails(b);
}

void BooksPage::showDetails(const Entity &entity)
{
    // Runtime polymorphism: 'entity' is a Book here, but the same call works
    // for Reader and BorrowRecord on their pages.
    QString text = entity.displayInfo();

    // A librarian also needs to know who is holding the copies right now. A
    // patron must not see that, so this is behind the same flag as the history.
    if (selfReaderID.isEmpty()) {
        const QList<BorrowRecord> out = system->getActiveRecordsByBook(entity.getID());
        if (out.isEmpty()) {
            text += "\n" + TR("Every copy is on the shelf.");
        } else {
            const QDate today = QDate::currentDate();
            QStringList lines;
            for (const BorrowRecord &r : out) {
                const Reader who = system->readerManager()->findReaderByID(r.getReaderID());
                const int late = r.lateDays(today);
                lines << QString("  • %1 — %2%3")
                             .arg(who.isValid()
                                      ? QString("%1 (%2)").arg(who.getFullName(),
                                                               who.getReaderID())
                                      : r.getReaderID(),
                                  QString(TR("due %1")).arg(r.getDueDate()),
                                  late > 0 ? QString(TR(", %1 day(s) late")).arg(late)
                                           : QString());
            }
            text += "\n" + QString(TR("Currently borrowed by:")) + "\n" + lines.join("\n");
        }
    }
    detailLabel->setText(text);
}

void BooksPage::onShowHistory()
{
    const int index = selectedIndex();
    if (index < 0)
        return;
    // A value copy, not a reference: the facade emits dataChanged, which
    // refills shownBooks and would leave a reference dangling.
    const Book b = shownBooks.at(index);

    RecordsDialog dialog(
        system,
        QString(TR("Borrow history — %1")).arg(b.getTitle()),
        QString(TR("%1 — \"%2\" by %3\n%4 of %5 copies are on the shelf right now."))
            .arg(b.getBookID(), b.getTitle(), b.getAuthor())
            .arg(b.getAvailableQuantity())
            .arg(b.getTotalQuantity()),
        system->getRecordsByBook(b.getBookID()), this);
    dialog.exec();
}

void BooksPage::onBorrowSelected()
{
    const int index = selectedIndex();
    if (index < 0 || selfReaderID.isEmpty())
        return;
    // A value copy, not a reference: the facade emits dataChanged, which
    // refills shownBooks and would leave a reference dangling.
    const Book b = shownBooks.at(index);

    QString error;
    // The facade runs every rule: limit, availability, duplicate title and the
    // overdue block. The patron screen only reports the outcome.
    if (!system->borrowBook(selfReaderID, b.getBookID(),
                            QDate::currentDate().toString("yyyy-MM-dd"), &error)) {
        QMessageBox::warning(this, TR("Cannot borrow"), error);
        return;
    }

    const QDate due = QDate::currentDate().addDays(system->loanPeriodDays());
    QMessageBox::information(
        this, TR("Borrow"),
        QString(TR("You borrowed \"%1\".\nPlease return it by %2."))
            .arg(b.getTitle(), due.toString("yyyy-MM-dd")));
}

bool BooksPage::validateForm(QString *errorOut)
{
    if (idEdit->text().trimmed().isEmpty()) {
        *errorOut = TR("Book ID must not be empty.");
        return false;
    }
    if (titleEdit->text().trimmed().isEmpty()) {
        *errorOut = TR("Title must not be empty.");
        return false;
    }
    if (authorEdit->text().trimmed().isEmpty()) {
        *errorOut = TR("Author must not be empty.");
        return false;
    }
    return true;
}

void BooksPage::onAdd()
{
    QString error;
    if (!validateForm(&error)) {
        QMessageBox::warning(this, TR("Invalid input"), error);
        return;
    }
    const int total = totalSpin->value();
    Book book(idEdit->text().trimmed(), titleEdit->text().trimmed(),
              authorEdit->text().trimmed(), categoryEdit->text().trimmed(),
              total, total);
    if (!system->bookManager()->addBook(book, &error)) {
        QMessageBox::warning(this, TR("Cannot add book"), error);
        return;
    }
    system->notifyDataChanged();
    onClearForm();
}

void BooksPage::onUpdate()
{
    QString error;
    if (!validateForm(&error)) {
        QMessageBox::warning(this, TR("Invalid input"), error);
        return;
    }
    Book book(idEdit->text().trimmed(), titleEdit->text().trimmed(),
              authorEdit->text().trimmed(), categoryEdit->text().trimmed(),
              totalSpin->value(), 0); // available is recomputed by BookManager
    if (!system->bookManager()->updateBook(book, &error)) {
        QMessageBox::warning(this, TR("Cannot update book"), error);
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
        this, TR("Delete book"),
        QString(TR("Really delete book \"%1\"?\nThis cannot be undone.")).arg(bookID));
    if (answer != QMessageBox::Yes)
        return;

    QString error;
    if (!system->bookManager()->deleteBook(bookID, &error)) {
        QMessageBox::warning(this, TR("Cannot delete book"), error);
        return;
    }
    system->notifyDataChanged();
    onClearForm();
}

void BooksPage::onClearForm()
{
    table->clearSelection();
    // The next free ID is filled in for the user instead of making them invent
    // one and find out it is taken only after pressing Add.
    idEdit->setText(system->bookManager()->nextBookID());
    idEdit->setReadOnly(false);
    titleEdit->clear();
    authorEdit->clear();
    categoryEdit->clear();
    totalSpin->setValue(1);
    updateButton->setEnabled(false);
    deleteButton->setEnabled(false);
    historyButton->setEnabled(false);
    borrowButton->setEnabled(false);
    reserveButton->setEnabled(false);
    detailLabel->setText(TR("Select a book to see its details."));
}

void BooksPage::onReserveSelected()
{
    const int index = selectedIndex();
    if (index < 0 || selfReaderID.isEmpty())
        return;
    // A value copy, not a reference: the facade emits dataChanged, which
    // refills shownBooks and would leave a reference dangling.
    const Book b = shownBooks.at(index);

    QString error;
    if (!system->reserveBook(selfReaderID, b.getBookID(), &error)) {
        QMessageBox::warning(this, TR("Reserve"), error);
        return;
    }
    const bool ready = b.isAvailable();
    QMessageBox::information(
        this, TR("Request"),
        ready ? QString(TR("\"%1\" is reserved for you.\n"
                           "Bring your slip to the desk to collect it."))
                    .arg(b.getTitle())
              : QString(TR("You are now in the queue for \"%1\".\n"
                           "When a copy comes back you will see it as ready in \"My books\"."))
                    .arg(b.getTitle()));
}
