#include "ui/MainWindow.h"

#include <QTabWidget>

#include "ui/DashboardPage.h"
#include "ui/BooksPage.h"
#include "ui/ReadersPage.h"
#include "ui/BorrowPage.h"

MainWindow::MainWindow(LibrarySystem *system, QWidget *parent)
    : QMainWindow(parent), system(system)
{
    setWindowTitle("Library Management System");
    resize(1150, 700);

    tabs = new QTabWidget(this);
    tabs->addTab(new DashboardPage(system, this), "  Dashboard  ");
    tabs->addTab(new BooksPage(system, this), "  Books  ");
    tabs->addTab(new ReadersPage(system, this), "  Readers  ");
    tabs->addTab(new BorrowPage(system, this), "  Borrow / Return  ");
    setCentralWidget(tabs);

    applyStyle();
}

void MainWindow::applyStyle()
{
    setStyleSheet(R"(
        QMainWindow { background: #eef1f6; }
        QTabWidget::pane { border: 1px solid #cfd6e0; background: #ffffff; }
        QTabBar::tab {
            padding: 8px 14px; background: #dde3ec; color: #2f3b52;
            border: 1px solid #cfd6e0; border-bottom: none;
            border-top-left-radius: 6px; border-top-right-radius: 6px;
        }
        QTabBar::tab:selected { background: #ffffff; font-weight: bold; }
        QGroupBox {
            border: 1px solid #cfd6e0; border-radius: 6px;
            margin-top: 12px; padding-top: 6px; font-weight: bold; color: #2f3b52;
        }
        QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 4px; }
        QTableWidget {
            background: #ffffff; color: #2f3b52; gridline-color: #e2e7ef;
            selection-background-color: #cfe0f7; selection-color: #17233a;
        }
        QLabel { color: #2f3b52; }
        QHeaderView::section {
            background: #43597d; color: white; padding: 6px; border: none; font-weight: bold;
        }
        QPushButton {
            background: #43597d; color: white; border: none; border-radius: 5px;
            padding: 7px 16px; font-weight: bold;
        }
        QPushButton:hover { background: #56719c; }
        QPushButton:disabled { background: #aab4c4; }
        QPushButton#dangerButton { background: #b34a4a; }
        QPushButton#dangerButton:hover { background: #cc6060; }
        QLineEdit, QSpinBox, QDateEdit, QComboBox {
            border: 1px solid #cfd6e0; border-radius: 4px; padding: 5px; background: white;
        }
        QLabel#statNumber { font-size: 32px; font-weight: bold; color: #2f3b52; }
        QLabel#statTitle { font-size: 12px; color: #5a6a85; }
        QFrame#statCard { background: white; border: 1px solid #cfd6e0; border-radius: 8px; }
        QLabel#detailBox {
            background: #f7f9fc; border: 1px dashed #b8c2d4; border-radius: 6px;
            padding: 8px; color: #2f3b52;
        }
    )");
}
