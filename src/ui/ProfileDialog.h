#ifndef PROFILEDIALOG_H
#define PROFILEDIALOG_H

#include <QByteArray>
#include <QDialog>

#include "managers/LibrarySystem.h"
#include "models/Account.h"

class QLabel;
class QLineEdit;

// "My profile" for whoever is signed in.
//
// A reader edits their own name, phone, email and picture — the picture and
// contact details live on their Reader row. A librarian has no Reader row, so
// their picture is stored on the Account instead.
class ProfileDialog : public QDialog
{
    Q_OBJECT

public:
    ProfileDialog(LibrarySystem *system, const Account &account, QWidget *parent = nullptr);

private slots:
    void onChangePhoto();
    void onRemovePhoto();
    void onSave();

private:
    void showPhoto();

    LibrarySystem *system;
    Account account;
    QByteArray photo;      // pending picture, written only when Save is pressed
    bool photoChanged;

    QLabel *photoLabel;
    QLabel *roleLabel;
    QLabel *errorLabel;
    QLineEdit *nameEdit;
    QLineEdit *phoneEdit;
    QLineEdit *emailEdit;
};

#endif // PROFILEDIALOG_H
