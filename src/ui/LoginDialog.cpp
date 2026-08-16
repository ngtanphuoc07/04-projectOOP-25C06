#include "i18n/Lang.h"
#include "ui/LoginDialog.h"

#include <QFormLayout>
#include <QFrame>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QAction>
#include <QRegularExpression>
#include <QTabWidget>
#include <QVBoxLayout>

#include "managers/ReaderManager.h"
#include "ui/Theme.h"

LoginDialog::LoginDialog(LibrarySystem *system, QWidget *parent)
    : QDialog(parent), system(system)
{
    setWindowTitle(TR("Library Management System — Sign in"));
    setModal(true);
    setMinimumWidth(430);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(22, 20, 22, 20);
    layout->setSpacing(12);

    auto *heading = new QLabel(TR("Library Management System"), this);
    heading->setObjectName("brandTitle");
    auto *subHeading = new QLabel(TR("Sign in to continue, or register as a new reader."), this);
    subHeading->setObjectName("brandSub");
    layout->addWidget(heading);
    layout->addWidget(subHeading);

    tabs = new QTabWidget(this);
    tabs->addTab(buildLoginTab(), TR("Sign in"));
    tabs->addTab(buildRegisterTab(), TR("Register"));
    layout->addWidget(tabs);

    auto *hint = new QLabel(TR("Librarian demo account: admin / 123"), this);
    hint->setObjectName("hintBox");
    layout->addWidget(hint);

    // Put the caret in the first field of whichever tab is showing, so the user
    // can start typing straight away without clicking first.
    connect(tabs, &QTabWidget::currentChanged, this, [this](int index) {
        if (index == 0)
            loginUser->setFocus();
        else
            regUser->setFocus();
    });
    loginUser->setFocus();

    setStyleSheet(Theme::dialogStyleSheet());

}

QWidget *LoginDialog::buildLoginTab()
{
    auto *page = new QWidget(this);
    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(16, 16, 16, 16);

    auto *form = new QFormLayout();
    loginUser = new QLineEdit(page);
    loginUser->setPlaceholderText(TR("admin"));
    loginPass = new QLineEdit(page);
    loginPass->setEchoMode(QLineEdit::Password);
    addRevealButton(loginPass);
    form->addRow(TR("Username:"), loginUser);
    form->addRow(TR("Password:"), loginPass);
    outer->addLayout(form);

    loginError = new QLabel(page);
    loginError->setObjectName("formError");
    loginError->setWordWrap(true);
    loginError->hide();
    outer->addWidget(loginError);

    auto *signIn = new QPushButton(TR("Sign in"), page);
    signIn->setDefault(true);
    outer->addWidget(signIn);

    auto *toRegister = new QPushButton(TR("No account yet? Register as a reader"), page);
    toRegister->setObjectName("linkButton");
    toRegister->setObjectName("linkButton");
    outer->addWidget(toRegister);
    outer->addStretch(1);

    connect(signIn, &QPushButton::clicked, this, &LoginDialog::onLogin);
    // Enter in either field signs in, so the whole dialog is keyboard-only.
    connect(loginUser, &QLineEdit::returnPressed, this, &LoginDialog::onLogin);
    connect(loginPass, &QLineEdit::returnPressed, this, &LoginDialog::onLogin);
    connect(toRegister, &QPushButton::clicked, this, [this] { tabs->setCurrentIndex(1); });

    return page;
}

QWidget *LoginDialog::buildRegisterTab()
{
    auto *page = new QWidget(this);
    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(16, 16, 16, 16);

    auto *form = new QFormLayout();
    regUser = new QLineEdit(page);
    regUser->setPlaceholderText(TR("3–20 letters, digits, dot or underscore"));
    regPass = new QLineEdit(page);
    regPass->setEchoMode(QLineEdit::Password);
    addRevealButton(regPass);
    regPass2 = new QLineEdit(page);
    regPass2->setEchoMode(QLineEdit::Password);
    addRevealButton(regPass2);
    regName = new QLineEdit(page);
    regPhone = new QLineEdit(page);
    regPhone->setPlaceholderText(TR("e.g. 0901234567 or +84 901 234 567"));
    regEmail = new QLineEdit(page);
    regEmail->setPlaceholderText(TR("name@example.com"));

    form->addRow(TR("Username:"), regUser);
    form->addRow(TR("Password:"), regPass);
    form->addRow(TR("Repeat password:"), regPass2);
    form->addRow(TR("Full name:"), regName);
    form->addRow(TR("Phone:"), regPhone);
    form->addRow(TR("Email:"), regEmail);
    outer->addLayout(form);

    regError = new QLabel(page);
    regError->setObjectName("formError");
    regError->setWordWrap(true);
    regError->hide();
    outer->addWidget(regError);

    auto *createButton = new QPushButton(TR("Create reader account"), page);
    outer->addWidget(createButton);
    outer->addStretch(1);

    connect(createButton, &QPushButton::clicked, this, &LoginDialog::onRegister);
    connect(regEmail, &QLineEdit::returnPressed, this, &LoginDialog::onRegister);

    return page;
}

void LoginDialog::addRevealButton(QLineEdit *field)
{
    // An eye inside the field toggles between dots and plain text, so a typo in
    // a password you cannot see does not turn into a failed sign-in.
    //
    // QLineEdit paints the action's ICON and never its text, so the first
    // attempt — an empty QIcon plus setText() — drew nothing at all.
    auto *reveal = field->addAction(Theme::eyeIcon(false), QLineEdit::TrailingPosition);
    reveal->setToolTip(TR("Show password"));
    connect(reveal, &QAction::triggered, this, [field, reveal] {
        const bool nowVisible = field->echoMode() == QLineEdit::Password;
        field->setEchoMode(nowVisible ? QLineEdit::Normal : QLineEdit::Password);
        reveal->setIcon(Theme::eyeIcon(nowVisible));
        reveal->setToolTip(nowVisible ? TR("Hide password") : TR("Show password"));
    });
}

void LoginDialog::showError(QLabel *label, const QString &message)
{
    label->setText(message);
    label->setVisible(!message.isEmpty());
}

void LoginDialog::onLogin()
{
    QString error;
    const Account found = system->accountManager()->login(loginUser->text(),
                                                          loginPass->text(), &error);
    if (!found.isValid()) {
        showError(loginError, error);
        loginPass->clear();
        loginPass->setFocus();
        return;
    }
    account = found;
    accept();
}

void LoginDialog::onRegister()
{
    showError(regError, QString());

    if (regPass->text() != regPass2->text()) {
        showError(regError, TR("The two passwords do not match."));
        return;
    }

    // Accept the phone number the way people actually write it and strip the
    // formatting here rather than rejecting the input.
    const QString phone = ReaderManager::normalisePhone(regPhone->text());
    if (phone.length() < 9 || phone.length() > 11) {
        showError(regError, TR("Phone must contain 9–11 digits."));
        return;
    }

    static const QRegularExpression emailPattern("^[\\w.+-]+@[\\w-]+(\\.[\\w-]+)+$");
    if (!emailPattern.match(regEmail->text().trimmed()).hasMatch()) {
        showError(regError, TR("Email address is not valid."));
        return;
    }

    QString error;
    if (!system->registerReaderAccount(regUser->text().trimmed(), regPass->text(),
                                       regName->text(), phone, regEmail->text(), &error)) {
        showError(regError, error);
        return;
    }

    QMessageBox::information(
        this, TR("Account created"),
        QString(TR("Welcome, %1!\n\nYour reader account \"%2\" is ready — you are now signed in."))
            .arg(regName->text().trimmed(), regUser->text().trimmed()));

    account = system->accountManager()->findAccountByUsername(regUser->text().trimmed());
    accept();
}

Account LoginDialog::loggedInAccount() const
{
    return account;
}
