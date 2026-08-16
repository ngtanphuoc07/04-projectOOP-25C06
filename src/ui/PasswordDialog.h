#ifndef PASSWORDDIALOG_H
#define PASSWORDDIALOG_H

#include <QDialog>

#include "managers/LibrarySystem.h"

class QLabel;
class QLineEdit;

// Lets the signed-in user change their own password. Reachable from the header
// by both roles — a librarian resetting a reader's password (Manage → Accounts)
// is a different thing: that one does not need the old password.
class PasswordDialog : public QDialog
{
    Q_OBJECT

public:
    PasswordDialog(LibrarySystem *system, const QString &username, QWidget *parent = nullptr);

private slots:
    void onSave();

private:
    LibrarySystem *system;
    QString username;

    QLineEdit *oldEdit;
    QLineEdit *newEdit;
    QLineEdit *confirmEdit;
    QLabel *errorLabel;
};

#endif // PASSWORDDIALOG_H
