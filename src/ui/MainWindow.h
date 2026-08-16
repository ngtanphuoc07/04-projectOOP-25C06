#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QList>
#include <QFrame>
#include <QMainWindow>

#include <functional>

#include "managers/LibrarySystem.h"
#include "models/Account.h"

class QPushButton;
class QMenu;
class QStackedWidget;
class QToolButton;
class QWidget;

// Main application window. Which tabs exist depends on the role of the account
// that signed in:
//   Librarian - Dashboard, Books, Readers, Borrow / Return.
//   Reader    - My books and a read-only catalogue they can borrow from.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(LibrarySystem *system, const Account &account, QWidget *parent = nullptr);

    // True when the window closed because the user pressed "Sign out" rather
    // than quitting; main() then shows the login dialog again.
    bool logoutRequested() const;

private slots:
    // Throws the whole interior away and builds it again in the new language.
    // Cheaper to reason about than asking every widget to re-apply its texts,
    // where one forgotten label would stay in the old language for good.
    void rebuildUi();
    // Shown once on sign-in when the reader is holding a late book.
    void warnIfOverdue();

private:
    // One destination in the sidebar. The page itself is built lazily so
    // rebuilding the window in another language costs nothing until shown.
    struct NavEntry
    {
        QString title;
        QString subtitle;
        std::function<QWidget *()> page;
    };

    QWidget *buildHeader();
    QMenu *buildAccountMenu();
    void refreshAvatar();
    QWidget *buildSidebar();
    QWidget *buildPages();
    void buildNavEntries();
    void showPage(int index);

    LibrarySystem *system;
    Account account;
    QWidget *sidebar = nullptr;
    QToolButton *avatarButton = nullptr;
    QStackedWidget *pages = nullptr;
    QList<NavEntry> navEntries;
    QList<QPushButton *> navButtons;
    bool logoutFlag;
};

#endif // MAINWINDOW_H
