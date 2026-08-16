#ifndef BOOKSPAGE_H
#define BOOKSPAGE_H

#include <QList>
#include <QWidget>

#include "managers/LibrarySystem.h"
#include "models/Book.h"

class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

// CRUD + search screen for books.
// With editable = false the whole edit form is hidden and the page becomes the
// catalogue a signed-in reader browses.
class BooksPage : public QWidget
{
    Q_OBJECT

public:
    // editable      - librarian mode: the add/update/delete form is shown.
    // selfReaderID  - non-empty in patron mode: enables "Borrow this book" for
    //                 that reader. Patrons never get the borrow-history button,
    //                 because it would reveal who else borrowed the title.
    explicit BooksPage(LibrarySystem *system, bool editable = true,
                       const QString &selfReaderID = QString(), QWidget *parent = nullptr);

private slots:
    void refresh();
    void onSearch();
    void onRowSelected();
    void onAdd();
    void onUpdate();
    void onDelete();
    void onClearForm();
    void onShowHistory();
    void onBorrowSelected();
    void onReserveSelected();

private:
    void fillTable(const QList<Book> &books);
    bool validateForm(QString *errorOut);
    void showDetails(const Entity &entity); // polymorphic detail panel
    int selectedIndex() const;              // row -> index into shownBooks

    LibrarySystem *system;
    bool editable;
    QString selfReaderID;
    QList<Book> shownBooks;

    QTableWidget *table;
    QLineEdit *searchEdit;
    QLineEdit *idEdit;
    QLineEdit *titleEdit;
    QLineEdit *authorEdit;
    QLineEdit *categoryEdit;
    QSpinBox *totalSpin;
    QGroupBox *formGroup;
    QPushButton *addButton;
    QPushButton *updateButton;
    QPushButton *deleteButton;
    QPushButton *historyButton;
    QPushButton *borrowButton;
    QPushButton *reserveButton;
    QLabel *detailLabel;
    QLabel *noticeLabel;
};

#endif // BOOKSPAGE_H
