#ifndef READERSPAGE_H
#define READERSPAGE_H

#include <QList>
#include <QWidget>

#include "managers/LibrarySystem.h"
#include "models/Reader.h"

class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;

// CRUD + search screen for readers, plus a per-reader view of the books they
// are currently holding and everything they have ever borrowed.
class ReadersPage : public QWidget
{
    Q_OBJECT

public:
    explicit ReadersPage(LibrarySystem *system, QWidget *parent = nullptr);

private slots:
    void refresh();
    void onSearch();
    void onRowSelected();
    void onAdd();
    void onUpdate();
    void onDelete();
    void onClearForm();
    void onShowLoans();
    void onShowDetail();

private:
    void fillTable(const QList<Reader> &readers);
    bool validateForm(QString *errorOut);
    void showDetails(const Entity &entity); // polymorphic detail panel
    int selectedIndex() const;              // row -> index into shownReaders

    LibrarySystem *system;
    QList<Reader> shownReaders;

    QTableWidget *table;
    QLineEdit *searchEdit;
    QLineEdit *idEdit;
    QLineEdit *nameEdit;
    QLineEdit *phoneEdit;
    QLineEdit *emailEdit;
    QPushButton *addButton;
    QPushButton *updateButton;
    QPushButton *deleteButton;
    QPushButton *loansButton;
    QPushButton *detailButton;
    QLabel *detailLabel;
};

#endif // READERSPAGE_H
