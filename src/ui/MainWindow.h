#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

#include "managers/LibrarySystem.h"

class QTabWidget;

// Main application window: a tab per functional area.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(LibrarySystem *system, QWidget *parent = nullptr);

private:
    void applyStyle();

    LibrarySystem *system;
    QTabWidget *tabs;
};

#endif // MAINWINDOW_H
