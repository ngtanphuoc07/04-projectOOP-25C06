#include "ui/AdminPage.h"

#include <QBuffer>
#include <QDateTime>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QTabWidget>
#include <QTableWidget>
#include <QVBoxLayout>

#include "i18n/Lang.h"
#include <QFileDialog>
#include <QImage>

#include "ui/ReaderDetailDialog.h"
#include "ui/Theme.h"
#include "ui/SuggestBox.h"
#include "util/SearchUtil.h"

AdminPage::AdminPage(LibrarySystem *system, const QString &currentUser, QWidget *parent)
    : QWidget(parent), system(system), currentUser(currentUser)
{
    setObjectName("pageBody");

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    auto *tabs = new QTabWidget(this);
    tabs->addTab(buildAccountsTab(), TR("Accounts"));
    tabs->addTab(buildRulesTab(), TR("Rules"));
    tabs->addTab(buildActivityTab(), TR("Activity"));
    layout->addWidget(tabs, 1);

    connect(system, &LibrarySystem::dataChanged, this, &AdminPage::refresh);
    refresh();
}

// ------------------------------------------------------------- Accounts

QWidget *AdminPage::buildAccountsTab()
{
    auto *page = new QWidget(this);
    page->setObjectName("pageBody");
    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(14, 14, 14, 14);
    outer->setSpacing(10);

    auto *hint = new QLabel(
        TR("A reader who forgot their password cannot recover it themselves — "
           "reset it here and tell them the new one."), page);
    hint->setObjectName("hintBox");
    hint->setWordWrap(true);
    outer->addWidget(hint);

    auto *searchRow = new QHBoxLayout();
    accountSearch = new QLineEdit(page);
    accountSearch->setPlaceholderText(
        TR("Search by username, name, reader ID, email or phone..."));
    accountSearch->setClearButtonEnabled(true);
    SuggestBox::attach(accountSearch, [this] {
        QStringList out;
        const QList<Account> all = system->accountManager()->getAllAccounts();
        for (const Account &a : all) {
            out << a.getUsername();
            if (a.getLinkedReaderID().isEmpty())
                continue;
            const Reader r = system->readerManager()->findReaderByID(a.getLinkedReaderID());
            if (r.isValid())
                out << r.getFullName() << r.getEmail() << r.getPhone();
        }
        return out;
    });
    searchRow->addWidget(accountSearch, 1);
    outer->addLayout(searchRow);
    connect(accountSearch, &QLineEdit::textChanged, this, [this] { refresh(); });

    accountsTable = new QTableWidget(0, 5, page);
    accountsTable->setHorizontalHeaderLabels(
        {TR("Username"), TR("Role"), TR("Reader"), TR("Failed attempts"), TR("Status")});
    accountsTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    accountsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    accountsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    accountsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    accountsTable->setAlternatingRowColors(true);
    accountsTable->verticalHeader()->setVisible(false);
    accountsTable->setSortingEnabled(true);
    accountsTable->setMinimumHeight(280);
    outer->addWidget(accountsTable, 1);

    auto *row = new QHBoxLayout();
    resetButton = new QPushButton(TR("Reset password"), page);
    editReaderButton = new QPushButton(TR("Edit reader details"), page);
    editReaderButton->setObjectName("secondaryButton");
    lockButton = new QPushButton(TR("Lock account"), page);
    lockButton->setObjectName("secondaryButton");
    unlockButton = new QPushButton(TR("Unlock"), page);
    unlockButton->setObjectName("secondaryButton");
    deleteButton = new QPushButton(TR("Delete account"), page);
    deleteButton->setObjectName("dangerButton");
    for (QPushButton *b : {resetButton, editReaderButton, lockButton,
                           unlockButton, deleteButton}) {
        b->setEnabled(false);
        row->addWidget(b);
    }
    row->addStretch(1);
    outer->addLayout(row);

    connect(accountsTable, &QTableWidget::itemSelectionChanged,
            this, &AdminPage::onAccountSelected);
    connect(resetButton, &QPushButton::clicked, this, &AdminPage::onResetPassword);
    connect(editReaderButton, &QPushButton::clicked, this, &AdminPage::onEditReader);
    connect(lockButton, &QPushButton::clicked, this, &AdminPage::onLock);
    connect(unlockButton, &QPushButton::clicked, this, &AdminPage::onUnlock);
    connect(deleteButton, &QPushButton::clicked, this, &AdminPage::onDeleteAccount);

    return page;
}

QString AdminPage::selectedUsername() const
{
    const int row = accountsTable->currentRow();
    if (row < 0)
        return QString();
    const QTableWidgetItem *item = accountsTable->item(row, 0);
    if (!item)
        return QString();
    const int index = item->data(Qt::UserRole).toInt();
    if (index < 0 || index >= shownAccounts.size())
        return QString();
    return shownAccounts.at(index).getUsername();
}

void AdminPage::onAccountSelected()
{
    const QString username = selectedUsername();
    const bool has = !username.isEmpty();
    resetButton->setEnabled(has);

    bool locked = false;
    for (const Account &a : shownAccounts) {
        if (a.getUsername() == username) {
            locked = a.isDisabled() || a.isLockedAt(QDateTime::currentDateTime());
            break;
        }
    }
    // Locking yourself out, or deleting the account you are signed in with,
    // would leave this very session pointing at nothing.
    // Only accounts backed by a reader record have details to edit.
    QString linked;
    for (const Account &a : shownAccounts) {
        if (a.getUsername() == username) {
            linked = a.getLinkedReaderID();
            break;
        }
    }
    editReaderButton->setEnabled(has && !linked.isEmpty());
    lockButton->setEnabled(has && !locked && username != currentUser);
    unlockButton->setEnabled(has && locked);
    deleteButton->setEnabled(has && username != currentUser);
}

void AdminPage::onEditReader()
{
    const QString username = selectedUsername();
    if (username.isEmpty())
        return;
    const Account a = system->accountManager()->findAccountByUsername(username);
    if (a.getLinkedReaderID().isEmpty()) {
        QMessageBox::information(this, TR("Edit reader details"),
                                 TR("This account has no reader record behind it."));
        return;
    }
    // The same window the Readers screen opens, with editing switched on.
    ReaderDetailDialog dialog(system, a.getLinkedReaderID(), true, this);
    dialog.exec();
}

void AdminPage::onLock()
{
    const QString username = selectedUsername();
    if (username.isEmpty())
        return;
    const auto answer = QMessageBox::question(
        this, TR("Lock account"),
        QString(TR("Lock \"%1\"?\nThey will not be able to sign in until you unlock it."))
            .arg(username));
    if (answer != QMessageBox::Yes)
        return;

    QString error;
    if (!system->accountManager()->lockAccount(username, currentUser, &error)) {
        QMessageBox::warning(this, TR("Lock account"), error);
        return;
    }
    system->notifyDataChanged();
}

void AdminPage::onResetPassword()
{
    const QString username = selectedUsername();
    if (username.isEmpty())
        return;

    bool ok = false;
    const QString pwd = QInputDialog::getText(
        this, TR("Reset password"),
        QString(TR("New password for \"%1\":\n(at least 6 characters, with a letter and a digit)"))
            .arg(username),
        QLineEdit::Normal, QString(), &ok);
    if (!ok || pwd.isEmpty())
        return;

    QString error;
    if (!system->accountManager()->resetPassword(username, pwd, &error)) {
        QMessageBox::warning(this, TR("Reset password"), error);
        return;
    }
    system->notifyDataChanged();
    QMessageBox::information(this, TR("Reset password"),
                             QString(TR("The password for \"%1\" has been changed."))
                                 .arg(username));
}

void AdminPage::onUnlock()
{
    const QString username = selectedUsername();
    if (username.isEmpty())
        return;
    QString error;
    if (!system->accountManager()->unlockAccount(username, &error)) {
        QMessageBox::warning(this, TR("Unlock"), error);
        return;
    }
    system->notifyDataChanged();
}

void AdminPage::onDeleteAccount()
{
    const QString username = selectedUsername();
    if (username.isEmpty())
        return;
    const auto answer = QMessageBox::question(
        this, TR("Delete account"),
        QString(TR("Delete the login \"%1\"?\nThe reader record itself is kept."))
            .arg(username));
    if (answer != QMessageBox::Yes)
        return;

    QString error;
    if (!system->accountManager()->deleteAccount(username, &error)) {
        QMessageBox::warning(this, TR("Delete account"), error);
        return;
    }
    system->notifyDataChanged();
}

// ---------------------------------------------------------------- Rules

QWidget *AdminPage::buildRulesTab()
{
    auto *page = new QWidget(this);
    page->setObjectName("pageBody");
    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(14, 14, 14, 14);
    outer->setSpacing(12);

    auto *hint = new QLabel(
        TR("These used to be fixed in the source code. Changing them here takes "
           "effect immediately and applies to new loans."), page);
    hint->setObjectName("hintBox");
    hint->setWordWrap(true);
    outer->addWidget(hint);

    auto *group = new QGroupBox(TR("Lending rules"), page);
    auto *form = new QFormLayout(group);
    form->setSpacing(10);

    limitSpin = new QSpinBox(group);       limitSpin->setRange(1, 50);
    loanSpin = new QSpinBox(group);        loanSpin->setRange(1, 365);
    fineSpin = new QSpinBox(group);        fineSpin->setRange(0, 1000000);
    fineSpin->setSingleStep(1000);
    lostSpin = new QSpinBox(group);        lostSpin->setRange(1, 3650);
    renewCountSpin = new QSpinBox(group);  renewCountSpin->setRange(0, 10);
    renewDaysSpin = new QSpinBox(group);   renewDaysSpin->setRange(1, 365);

    form->addRow(TR("Books per reader:"), limitSpin);
    form->addRow(TR("Loan period (days):"), loanSpin);
    form->addRow(TR("Fine per day (VND):"), fineSpin);
    form->addRow(TR("Treat as not returned after (days):"), lostSpin);
    form->addRow(TR("Renewals allowed:"), renewCountSpin);
    form->addRow(TR("Days added per renewal:"), renewDaysSpin);
    outer->addWidget(group);

    auto *save = new QPushButton(TR("Save rules"), page);
    auto *row = new QHBoxLayout();
    row->addWidget(save);
    row->addStretch(1);
    outer->addLayout(row);

    // ---- the library's payment QR
    auto *qrGroup = new QGroupBox(TR("Payment QR code"), page);
    auto *qrLayout = new QHBoxLayout(qrGroup);

    qrPreview = new QLabel(qrGroup);
    qrPreview->setFixedSize(150, 150);
    qrPreview->setAlignment(Qt::AlignCenter);
    qrLayout->addWidget(qrPreview);

    auto *qrSide = new QVBoxLayout();
    auto *qrHint = new QLabel(
        TR("Upload the library's own MoMo or VietQR image. Readers see it on the "
           "payment screen. Leave it empty and the app draws a code instead."), qrGroup);
    qrHint->setObjectName("hintBox");
    qrHint->setWordWrap(true);
    qrSide->addWidget(qrHint);

    auto *qrButtons = new QHBoxLayout();
    auto *chooseQr = new QPushButton(TR("Choose image..."), qrGroup);
    auto *clearQr = new QPushButton(TR("Remove image"), qrGroup);
    clearQr->setObjectName("secondaryButton");
    qrButtons->addWidget(chooseQr);
    qrButtons->addWidget(clearQr);
    qrButtons->addStretch(1);
    qrSide->addLayout(qrButtons);
    qrSide->addStretch(1);
    qrLayout->addLayout(qrSide, 1);
    outer->addWidget(qrGroup);

    connect(chooseQr, &QPushButton::clicked, this, &AdminPage::onChooseQr);
    connect(clearQr, &QPushButton::clicked, this, &AdminPage::onClearQr);

    dbPathLabel = new QLabel(page);
    dbPathLabel->setObjectName("pageSub");
    dbPathLabel->setWordWrap(true);
    outer->addWidget(dbPathLabel);

    outer->addStretch(1);

    connect(save, &QPushButton::clicked, this, &AdminPage::onSaveRules);
    return page;
}

void AdminPage::onChooseQr()
{
    const QString path = QFileDialog::getOpenFileName(
        this, TR("Choose the payment QR image"), QString(),
        TR("Images (*.png *.jpg *.jpeg *.bmp *.webp)"));
    if (path.isEmpty())
        return;

    QImage image(path);
    if (image.isNull()) {
        QMessageBox::warning(this, TR("Payment QR code"),
                             TR("That file could not be read as an image."));
        return;
    }
    // Shrink oversized photos: the picture only ever appears at ~240 px, and
    // the database carries it in every backup.
    if (image.width() > 900 || image.height() > 900)
        image = image.scaled(900, 900, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QByteArray png;
    QBuffer buffer(&png);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) {
        QMessageBox::warning(this, TR("Payment QR code"),
                             TR("That image could not be converted."));
        return;
    }

    if (!system->setPaymentQrImage(png)) {
        QMessageBox::warning(this, TR("Payment QR code"),
                             TR("Database error while saving the image."));
        return;
    }
    QMessageBox::information(this, TR("Payment QR code"),
                             TR("Readers will now see this code on the payment screen."));
}

void AdminPage::onClearQr()
{
    if (system->paymentQrImage().isEmpty())
        return;
    system->setPaymentQrImage(QByteArray());
}

void AdminPage::onSaveRules()
{
    LibraryConfig cfg;
    cfg.maxBooksPerReader = limitSpin->value();
    cfg.loanPeriodDays = loanSpin->value();
    cfg.finePerDay = fineSpin->value();
    cfg.lostAfterDays = lostSpin->value();
    cfg.maxRenewals = renewCountSpin->value();
    cfg.renewalDays = renewDaysSpin->value();
    system->setConfig(cfg);
    QMessageBox::information(this, TR("Save rules"), TR("The new rules have been saved."));
}

// ------------------------------------------------------------- Activity

QWidget *AdminPage::buildActivityTab()
{
    auto *page = new QWidget(this);
    page->setObjectName("pageBody");
    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(14, 14, 14, 14);
    outer->setSpacing(10);

    auto *hint = new QLabel(
        TR("Every change is recorded here with the account that made it, so a "
           "mistaken deletion can be traced."), page);
    hint->setObjectName("hintBox");
    hint->setWordWrap(true);
    outer->addWidget(hint);

    auditTable = new QTableWidget(0, 4, page);
    auditTable->setHorizontalHeaderLabels({TR("When"), TR("User"), TR("Action"), TR("Details")});
    auditTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    auditTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    auditTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    auditTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    auditTable->setAlternatingRowColors(true);
    auditTable->verticalHeader()->setVisible(false);
    auditTable->setMinimumHeight(320);
    outer->addWidget(auditTable, 1);

    return page;
}

// ---------------------------------------------------------------- refresh

void AdminPage::refresh()
{
    // ---- accounts
    // Searching an account also looks at the reader behind it, so a librarian
    // can find a login by the person's name, e-mail or phone number.
    const QString keyword = accountSearch->text().trimmed();
    shownAccounts.clear();
    const QList<Account> everyAccount = system->accountManager()->getAllAccounts();
    for (const Account &a : everyAccount) {
        if (keyword.isEmpty()) {
            shownAccounts.append(a);
            continue;
        }
        QStringList fields{a.getUsername(), a.roleName(), a.getLinkedReaderID()};
        if (!a.getLinkedReaderID().isEmpty()) {
            const Reader r = system->readerManager()->findReaderByID(a.getLinkedReaderID());
            if (r.isValid())
                fields << r.getFullName() << r.getEmail() << r.getPhone();
        }
        if (SearchUtil::matches(fields, keyword))
            shownAccounts.append(a);
    }
    const QDateTime now = QDateTime::currentDateTime();

    accountsTable->setSortingEnabled(false);
    accountsTable->setRowCount(0);
    for (int i = 0; i < shownAccounts.size(); ++i) {
        const Account &a = shownAccounts.at(i);
        const int row = accountsTable->rowCount();
        accountsTable->insertRow(row);

        auto *nameItem = new QTableWidgetItem(a.getUsername());
        nameItem->setData(Qt::UserRole, i);
        accountsTable->setItem(row, 0, nameItem);
        accountsTable->setItem(row, 1, new QTableWidgetItem(a.roleName()));
        accountsTable->setItem(row, 2, new QTableWidgetItem(
            a.getLinkedReaderID().isEmpty() ? QStringLiteral("—") : a.getLinkedReaderID()));

        auto *failItem = new QTableWidgetItem();
        failItem->setData(Qt::DisplayRole, a.getFailedAttempts());
        accountsTable->setItem(row, 3, failItem);

        QString state = TR("Active");
        QColor colour = Theme::success();
        if (a.isDisabled()) {
            state = TR("Locked by a librarian");
            colour = Theme::critical();
        } else if (a.isLockedAt(now)) {
            state = QString(TR("Locked for %1 more minute(s)"))
                        .arg((a.secondsLockedAt(now) + 59) / 60);
            colour = Theme::danger();
        }
        auto *stateItem = new QTableWidgetItem(state);
        stateItem->setForeground(QBrush(colour));
        accountsTable->setItem(row, 4, stateItem);
    }
    accountsTable->setSortingEnabled(true);
    onAccountSelected();

    // ---- rules
    const LibraryConfig cfg = system->config();
    limitSpin->setValue(cfg.maxBooksPerReader);
    loanSpin->setValue(cfg.loanPeriodDays);
    fineSpin->setValue(cfg.finePerDay);
    lostSpin->setValue(cfg.lostAfterDays);
    renewCountSpin->setValue(cfg.maxRenewals);
    renewDaysSpin->setValue(cfg.renewalDays);
    dbPathLabel->setText(QString(TR("Database file: %1"))
                             .arg(DatabaseManager::instance()->databasePath()));

    const QByteArray qr = system->paymentQrImage();
    if (qr.isEmpty()) {
        qrPreview->setPixmap(QPixmap());
        qrPreview->setText(TR("No image — a code is drawn instead"));
        qrPreview->setWordWrap(true);
    } else {
        QPixmap pm;
        pm.loadFromData(qr, "PNG");
        qrPreview->setPixmap(pm.scaled(146, 146, Qt::KeepAspectRatio,
                                       Qt::SmoothTransformation));
    }

    // ---- activity
    const QList<AuditEntry> entries = DatabaseManager::instance()->getAuditEntries(300);
    auditTable->setRowCount(0);
    for (const AuditEntry &e : entries) {
        const int row = auditTable->rowCount();
        auditTable->insertRow(row);
        auditTable->setItem(row, 0, new QTableWidgetItem(
            QDateTime::fromString(e.at, Qt::ISODate).toString("yyyy-MM-dd HH:mm:ss")));
        auditTable->setItem(row, 1, new QTableWidgetItem(e.username));
        auditTable->setItem(row, 2, new QTableWidgetItem(e.action));
        auditTable->setItem(row, 3, new QTableWidgetItem(e.details));
    }
}
