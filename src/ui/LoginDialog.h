#ifndef LOGINDIALOG_H
#define LOGINDIALOG_H

#include <QDialog>
#include <QIcon>

#include "managers/LibrarySystem.h"
#include "models/Account.h"

class QLabel;
class QLineEdit;
class QTabWidget;

// Sign-in gate shown before the main window. Two tabs:
//   "Sign in"  - existing accounts (librarian or reader).
//   "Register" - a visitor creates a reader account for themselves; the
//                matching Reader row is created at the same time by
//                LibrarySystem::registerReaderAccount.
class LoginDialog : public QDialog
{
    Q_OBJECT

public:
    explicit LoginDialog(LibrarySystem *system, QWidget *parent = nullptr);

    // Valid only after exec() returned Accepted.
    Account loggedInAccount() const;

private slots:
    void onLogin();
    void onRegister();

private:
    QWidget *buildLoginTab();
    QWidget *buildRegisterTab();
    void showError(QLabel *label, const QString &message);
    // Adds the eye toggle that reveals what was typed.
    void addRevealButton(QLineEdit *field);

    LibrarySystem *system;
    Account account;

    QTabWidget *tabs;

    QLineEdit *loginUser;
    QLineEdit *loginPass;
    QLabel *loginError;

    QLineEdit *regUser;
    QLineEdit *regPass;
    QLineEdit *regPass2;
    QLineEdit *regName;
    QLineEdit *regPhone;
    QLineEdit *regEmail;
    QLabel *regError;
};

#endif // LOGINDIALOG_H
