#include "ui/PasswordDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>

#include "i18n/Lang.h"
#include "ui/Theme.h"

PasswordDialog::PasswordDialog(LibrarySystem *system, const QString &username, QWidget *parent)
    : QDialog(parent), system(system), username(username)
{
    setWindowTitle(TR("Change my password"));
    setModal(true);
    setMinimumWidth(400);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(12);

    auto *heading = new QLabel(QString(TR("Change the password for \"%1\"")).arg(username), this);
    heading->setObjectName("dialogHeading");
    layout->addWidget(heading);

    auto *hint = new QLabel(TR("At least 6 characters, with a letter and a digit."), this);
    hint->setObjectName("hintBox");
    hint->setWordWrap(true);
    layout->addWidget(hint);

    auto *form = new QFormLayout();
    oldEdit = new QLineEdit(this);
    oldEdit->setEchoMode(QLineEdit::Password);
    newEdit = new QLineEdit(this);
    newEdit->setEchoMode(QLineEdit::Password);
    confirmEdit = new QLineEdit(this);
    confirmEdit->setEchoMode(QLineEdit::Password);
    form->addRow(TR("Current password:"), oldEdit);
    form->addRow(TR("New password:"), newEdit);
    form->addRow(TR("Repeat new password:"), confirmEdit);
    layout->addLayout(form);

    errorLabel = new QLabel(this);
    errorLabel->setObjectName("formError");
    errorLabel->setWordWrap(true);
    errorLabel->hide();
    layout->addWidget(errorLabel);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel, this);
    buttons->button(QDialogButtonBox::Save)->setText(TR("Save"));
    buttons->button(QDialogButtonBox::Cancel)->setText(TR("Cancel"));
    buttons->button(QDialogButtonBox::Cancel)->setObjectName("secondaryButton");
    layout->addWidget(buttons);

    setStyleSheet(Theme::dialogStyleSheet());

    connect(buttons, &QDialogButtonBox::accepted, this, &PasswordDialog::onSave);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(confirmEdit, &QLineEdit::returnPressed, this, &PasswordDialog::onSave);

    oldEdit->setFocus();
}

void PasswordDialog::onSave()
{
    errorLabel->hide();

    if (newEdit->text() != confirmEdit->text()) {
        errorLabel->setText(TR("The two passwords do not match."));
        errorLabel->show();
        return;
    }

    QString error;
    if (!system->accountManager()->changePassword(username, oldEdit->text(),
                                                  newEdit->text(), &error)) {
        errorLabel->setText(error);
        errorLabel->show();
        oldEdit->clear();
        oldEdit->setFocus();
        return;
    }

    QMessageBox::information(this, TR("Change my password"),
                             TR("Your password has been changed."));
    accept();
}
