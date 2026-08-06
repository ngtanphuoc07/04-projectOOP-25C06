#ifndef DASHBOARDPAGE_H
#define DASHBOARDPAGE_H

#include <QWidget>

#include "managers/LibrarySystem.h"

class QLabel;
class QTableWidget;

// Statistics / report screen: stat cards + list of books currently out.
// Subscribes to LibrarySystem::dataChanged (Observer pattern) so the numbers
// are always up to date.
class DashboardPage : public QWidget
{
    Q_OBJECT

public:
    explicit DashboardPage(LibrarySystem *system, QWidget *parent = nullptr);

private slots:
    void refresh();

private:
    QWidget *makeStatCard(const QString &title, QLabel *&numberLabel);

    LibrarySystem *system;
    QLabel *totalBooksLabel;
    QLabel *totalReadersLabel;
    QLabel *borrowedLabel;
    QLabel *availableLabel;
    QTableWidget *activeTable;
};

#endif // DASHBOARDPAGE_H
