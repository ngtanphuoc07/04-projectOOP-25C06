#ifndef BOOKSPAGE_H
#define BOOKSPAGE_H

#include <QList>
#include <QWidget>

#include "managers/LibrarySystem.h"
#include "models/Book.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QTableWidget;

// CRUD + search screen for books.
class BooksPage : public QWidget
{
    Q_OBJECT

public:
    explicit BooksPage(LibrarySystem *system, QWidget *parent = nullptr);

private slots:
    void refresh();
    void onSearch();
    void onRowSelected();
    void onAdd();
    void onUpdate();
    void onDelete();
    void onClearForm();

private:
    void fillTable(const QList<Book> &books);
    bool validateForm(QString *errorOut);
    void showDetails(const Entity &entity); // polymorphic detail panel

    LibrarySystem *system;
    QList<Book> shownBooks;

    QTableWidget *table;
    QLineEdit *searchEdit;
    QLineEdit *idEdit;
    QLineEdit *titleEdit;
    QLineEdit *authorEdit;
    QLineEdit *categoryEdit;
    QSpinBox *totalSpin;
    QPushButton *addButton;
    QPushButton *updateButton;
    QPushButton *deleteButton;
    QLabel *detailLabel;
};

#endif // BOOKSPAGE_H
