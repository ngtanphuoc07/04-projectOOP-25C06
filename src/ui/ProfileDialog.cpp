#include "ui/ProfileDialog.h"

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QVBoxLayout>

#include "i18n/Lang.h"
#include "managers/ReaderManager.h"
#include "ui/Theme.h"
#include "util/AvatarUtil.h"

ProfileDialog::ProfileDialog(LibrarySystem *system, const Account &account, QWidget *parent)
    : QDialog(parent), system(system), account(account), photoChanged(false)
{
    setWindowTitle(TR("My profile"));
    setMinimumWidth(470);

    const bool isReader = !account.getLinkedReaderID().isEmpty();
    Reader me;
    if (isReader) {
        me = system->readerManager()->findReaderByID(account.getLinkedReaderID());
        photo = me.getAvatar();
    } else {
        photo = account.getAvatar();
    }

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(20, 18, 20, 18);
    layout->setSpacing(14);

    // ------------------------------------------------------ picture + name
    auto *card = new QGroupBox(this);
    auto *row = new QHBoxLayout(card);
    row->setContentsMargins(14, 12, 14, 12);
    row->setSpacing(16);

    photoLabel = new QLabel(card);
    photoLabel->setFixedSize(104, 104);
    row->addWidget(photoLabel);

    auto *col = new QVBoxLayout();
    col->setSpacing(6);
    auto *who = new QLabel(account.getUsername(), card);
    who->setObjectName("dialogHeading");
    roleLabel = new QLabel(card);
    roleLabel->setObjectName("dialogSummary");
    roleLabel->setText(isReader
                           ? QString("%1 · %2").arg(account.roleName(),
                                                    account.getLinkedReaderID())
                           : account.roleName());
    col->addWidget(who);
    col->addWidget(roleLabel);

    auto *photoButtons = new QHBoxLayout();
    auto *change = new QPushButton(TR("Change photo"), card);
    change->setObjectName("secondaryButton");
    auto *remove = new QPushButton(TR("Remove photo"), card);
    remove->setObjectName("secondaryButton");
    photoButtons->addWidget(change);
    photoButtons->addWidget(remove);
    photoButtons->addStretch(1);
    col->addLayout(photoButtons);
    col->addStretch(1);
    row->addLayout(col, 1);
    layout->addWidget(card);

    // ---------------------------------------------------- contact details
    nameEdit = new QLineEdit(this);
    phoneEdit = new QLineEdit(this);
    emailEdit = new QLineEdit(this);

    if (isReader) {
        auto *details = new QGroupBox(TR("My details"), this);
        auto *form = new QFormLayout(details);
        form->setSpacing(9);
        nameEdit->setText(me.getFullName());
        phoneEdit->setText(me.getPhone());
        emailEdit->setText(me.getEmail());
        form->addRow(TR("Full name:"), nameEdit);
        form->addRow(TR("Phone:"), phoneEdit);
        form->addRow(TR("Email:"), emailEdit);
        layout->addWidget(details);
    } else {
        // Nothing but the picture to edit: a librarian account has no reader
        // record behind it, so there is no name or contact detail to change.
        nameEdit->hide();
        phoneEdit->hide();
        emailEdit->hide();
        auto *note = new QLabel(
            TR("A librarian account has no reader record, so only the picture "
               "can be changed here. Use the Password button to change your password."),
            this);
        note->setObjectName("hintBox");
        note->setWordWrap(true);
        layout->addWidget(note);
    }

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

    connect(change, &QPushButton::clicked, this, &ProfileDialog::onChangePhoto);
    connect(remove, &QPushButton::clicked, this, &ProfileDialog::onRemovePhoto);
    connect(buttons, &QDialogButtonBox::accepted, this, &ProfileDialog::onSave);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    showPhoto();
}

void ProfileDialog::showPhoto()
{
    const QString fallback = nameEdit->text().isEmpty() ? account.getUsername()
                                                        : nameEdit->text();
    photoLabel->setPixmap(AvatarUtil::toPixmap(photo, 104, fallback));
}

void ProfileDialog::onChangePhoto()
{
    QString error;
    const QByteArray picked = AvatarUtil::pickImage(this, &error);
    if (picked.isEmpty()) {
        if (!error.isEmpty())
            QMessageBox::warning(this, TR("Change photo"), error);
        return;                          // cancelled
    }
    photo = picked;
    photoChanged = true;
    showPhoto();
}

void ProfileDialog::onRemovePhoto()
{
    if (photo.isEmpty())
        return;
    photo.clear();
    photoChanged = true;
    showPhoto();
}

void ProfileDialog::onSave()
{
    errorLabel->hide();
    const QString readerID = account.getLinkedReaderID();

    if (readerID.isEmpty()) {
        // Librarian: only the picture, which lives on the account.
        if (photoChanged) {
            Account updated = account;
            updated.setAvatar(photo);
            if (!DatabaseManager::instance()->updateAccount(updated)) {
                errorLabel->setText(TR("Database error while saving the profile picture."));
                errorLabel->show();
                return;
            }
            system->audit("AVATAR", QString("account %1").arg(account.getUsername()));
            system->notifyDataChanged();
        }
        accept();
        return;
    }

    // Reader: validate exactly like the librarian's reader form does.
    if (nameEdit->text().trimmed().isEmpty()) {
        errorLabel->setText(TR("Full name must not be empty."));
        errorLabel->show();
        return;
    }
    const QString phone = ReaderManager::normalisePhone(phoneEdit->text());
    if (phone.length() < 9 || phone.length() > 11) {
        errorLabel->setText(TR("Phone must contain 9–11 digits."));
        errorLabel->show();
        return;
    }
    static const QRegularExpression emailPattern("^[\\w.+-]+@[\\w-]+(\\.[\\w-]+)+$");
    if (!emailPattern.match(emailEdit->text().trimmed()).hasMatch()) {
        errorLabel->setText(TR("Email address is not valid."));
        errorLabel->show();
        return;
    }

    QString error;
    Reader updated(readerID, nameEdit->text().trimmed(), phone,
                   emailEdit->text().trimmed(), 0);
    // updateReader keeps borrowedCount and the stored avatar, so the picture is
    // saved separately below.
    if (!system->readerManager()->updateReader(updated, &error)) {
        errorLabel->setText(error);
        errorLabel->show();
        return;
    }
    if (photoChanged && !system->readerManager()->updateAvatar(readerID, photo, &error)) {
        errorLabel->setText(error);
        errorLabel->show();
        return;
    }

    system->audit("PROFILE", QString("reader %1 updated their own details").arg(readerID));
    system->notifyDataChanged();
    QMessageBox::information(this, TR("My profile"), TR("Your profile has been saved."));
    accept();
}
