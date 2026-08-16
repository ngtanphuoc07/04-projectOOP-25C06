#ifndef ADMINPAGE_H
#define ADMINPAGE_H

#include <QList>
#include <QWidget>

#include "managers/LibrarySystem.h"
#include "models/Account.h"

class QLabel;
class QLineEdit;
class QSpinBox;
class QTableWidget;
class QPushButton;

// Librarian-only screen with three jobs that used to have no home at all:
//   * Accounts - reset a forgotten password, unlock, delete
//   * Rules    - the business parameters, previously compiled in
//   * Activity - the audit trail
class AdminPage : public QWidget
{
    Q_OBJECT

public:
    AdminPage(LibrarySystem *system, const QString &currentUser, QWidget *parent = nullptr);

private slots:
    void refresh();
    void onAccountSelected();
    void onResetPassword();
    void onEditReader();
    void onLock();
    void onUnlock();
    void onDeleteAccount();
    void onSaveRules();
    void onChooseQr();
    void onClearQr();

private:
    QWidget *buildAccountsTab();
    QWidget *buildRulesTab();
    QWidget *buildActivityTab();
    QString selectedUsername() const;

    LibrarySystem *system;
    QString currentUser;
    QList<Account> shownAccounts;

    QLineEdit *accountSearch;
    QTableWidget *accountsTable;
    QPushButton *resetButton;
    QPushButton *editReaderButton;
    QPushButton *lockButton;
    QPushButton *unlockButton;
    QPushButton *deleteButton;

    QSpinBox *limitSpin;
    QSpinBox *loanSpin;
    QSpinBox *fineSpin;
    QSpinBox *lostSpin;
    QSpinBox *renewCountSpin;
    QSpinBox *renewDaysSpin;

    QTableWidget *auditTable;
    QLabel *dbPathLabel;
    QLabel *qrPreview;
};

#endif // ADMINPAGE_H
