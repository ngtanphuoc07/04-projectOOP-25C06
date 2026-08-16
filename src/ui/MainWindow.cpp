#include "ui/MainWindow.h"

#include <QComboBox>
#include <QActionGroup>
#include <QFontMetrics>
#include <QTimer>
#include <QToolButton>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QMenu>
#include <QPushButton>
#include <QScreen>
#include <QScrollArea>
#include <QStackedWidget>
#include <QVBoxLayout>

#include "i18n/Lang.h"
#include "ui/AdminPage.h"
#include "ui/BooksPage.h"
#include "ui/BorrowPage.h"
#include "ui/DashboardPage.h"
#include "ui/MyLoansPage.h"
#include "ui/PasswordDialog.h"
#include "ui/ProfileDialog.h"
#include "ui/ReadersPage.h"
#include "ui/Theme.h"
#include "util/AvatarUtil.h"

MainWindow::MainWindow(LibrarySystem *system, const Account &account, QWidget *parent)
    : QMainWindow(parent), system(system), account(account), logoutFlag(false)
{
    // Never open larger than the screen: on a small or scaled display a fixed
    // window would push the stat cards and the Sign out button out of sight.
    const QRect avail = QGuiApplication::primaryScreen()->availableGeometry();
    resize(qMin(1180, avail.width() - 60), qMin(740, avail.height() - 60));

    setStyleSheet(Theme::appStyleSheet());
    rebuildUi();

    // A reader who is barred should learn why on sign-in, not by pressing a
    // button and getting a refusal.
    if (!account.isLibrarian() && !account.getLinkedReaderID().isEmpty())
        QTimer::singleShot(0, this, &MainWindow::warnIfOverdue);

    connect(Lang::instance(), &Lang::languageChanged, this, &MainWindow::rebuildUi);
}

void MainWindow::rebuildUi()
{
    setWindowTitle(QString("%1 — %2 (%3)")
                       .arg(TR("Library Management System"),
                            account.getUsername(), account.roleName()));

    buildNavEntries();

    auto *central = new QWidget(this);
    central->setObjectName("pageBody");
    auto *outer = new QVBoxLayout(central);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    outer->addWidget(buildHeader());

    // Sidebar on the left, the current screen on the right. A vertical list of
    // named destinations is easier to scan than a row of tabs, and it leaves
    // room for a description under each name.
    auto *body = new QWidget(central);
    body->setObjectName("pageBody");
    auto *bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);
    bodyLayout->addWidget(buildSidebar());
    bodyLayout->addWidget(buildPages(), 1);
    outer->addWidget(body, 1);

    // Replaces (and deletes) whatever central widget was there before.
    setCentralWidget(central);
}

QWidget *MainWindow::buildSidebar()
{
    sidebar = new QWidget(this);
    sidebar->setObjectName("sidebar");
    sidebar->setFixedWidth(248);

    auto *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(12, 16, 12, 16);
    layout->setSpacing(6);

    navButtons.clear();
    for (int i = 0; i < navEntries.size(); ++i) {
        const auto &entry = navEntries.at(i);
        auto *button = new QPushButton(sidebar);
        button->setObjectName("navButton");
        button->setCheckable(true);
        button->setCursor(Qt::PointingHandCursor);
        // Shorten the subtitle to whatever actually fits, with an ellipsis. A
        // long one used to run off the edge and simply lose its last letters.
        const QFontMetrics metrics(button->font());
        const int room = sidebar->width() - 2 * 12 - 2 * 13;   // margins + padding
        button->setText(QString("%1\n%2").arg(
            entry.title, metrics.elidedText(entry.subtitle, Qt::ElideRight, room)));
        button->setToolTip(entry.subtitle);   // the full text is always here
        layout->addWidget(button);
        navButtons.append(button);

        connect(button, &QPushButton::clicked, this, [this, i] { showPage(i); });
    }

    layout->addStretch(1);

    auto *hint = new QLabel(TR("Signed in as"), sidebar);
    hint->setObjectName("sidebarFoot");
    auto *whoami = new QLabel(account.getUsername(), sidebar);
    whoami->setObjectName("sidebarFootStrong");
    layout->addWidget(hint);
    layout->addWidget(whoami);

    return sidebar;
}

QWidget *MainWindow::buildPages()
{
    pages = new QStackedWidget(this);
    pages->setObjectName("pageBody");
    for (const auto &entry : navEntries) {
        // Each screen lives inside a scroll area, so a small window (or a big
        // font) never hides the buttons at the bottom — the content scrolls
        // instead of being squashed.
        auto *scroller = new QScrollArea(pages);
        scroller->setObjectName("pageScroll");
        scroller->setWidgetResizable(true);
        scroller->setFrameShape(QFrame::NoFrame);
        scroller->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroller->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scroller->setWidget(entry.page());
        pages->addWidget(scroller);
    }
    showPage(0);
    return pages;
}

void MainWindow::showPage(int index)
{
    if (!pages || index < 0 || index >= pages->count())
        return;
    pages->setCurrentIndex(index);
    for (int i = 0; i < navButtons.size(); ++i)
        navButtons.at(i)->setChecked(i == index);
}

QWidget *MainWindow::buildHeader()
{
    auto *bar = new QWidget(this);
    bar->setObjectName("appHeader");
    auto *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(18, 9, 14, 9);
    layout->setSpacing(12);

    auto *title = new QLabel(TR("Library Management System"), bar);
    title->setObjectName("appTitle");
    layout->addWidget(title);
    layout->addStretch(1);

    // One avatar button instead of five separate ones. Everything about "me"
    // — profile, password, language, theme, signing out — lives behind it,
    // which is where people have learned to look for it.
    avatarButton = new QToolButton(bar);
    avatarButton->setObjectName("avatarButton");
    avatarButton->setPopupMode(QToolButton::InstantPopup);
    avatarButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    avatarButton->setText(QString::fromUtf8("\u25BE"));   // ▾
    avatarButton->setIconSize(QSize(34, 34));
    avatarButton->setCursor(Qt::PointingHandCursor);
    avatarButton->setToolTip(account.getUsername());
    avatarButton->setMenu(buildAccountMenu());
    refreshAvatar();
    layout->addWidget(avatarButton);

    // Uploading a new picture should change it here too, without a restart.
    connect(system, &LibrarySystem::dataChanged, avatarButton, [this] { refreshAvatar(); });

    return bar;
}

void MainWindow::refreshAvatar()
{
    if (!avatarButton)
        return;

    // A reader's picture lives on their reader record; a librarian has none, so
    // theirs is stored on the account itself.
    const Account fresh = system->accountManager()->findAccountByUsername(account.getUsername());
    const Account &use = fresh.isValid() ? fresh : account;

    QByteArray picture = use.getAvatar();
    QString name = use.getUsername();
    if (!use.getLinkedReaderID().isEmpty()) {
        const Reader me = system->readerManager()->findReaderByID(use.getLinkedReaderID());
        if (me.isValid()) {
            picture = me.getAvatar();
            name = me.getFullName();
        }
    }
    avatarButton->setIcon(QIcon(AvatarUtil::toPixmap(picture, 34, name)));
}

QMenu *MainWindow::buildAccountMenu()
{
    auto *menu = new QMenu(this);
    menu->setObjectName("accountMenu");

    // Who you are, at the top — not clickable, just a reminder.
    QAction *whoami = menu->addAction(account.getUsername());
    whoami->setEnabled(false);
    QString subtitle = account.roleName();
    if (!account.getLinkedReaderID().isEmpty())
        subtitle += "  ·  " + account.getLinkedReaderID();
    QAction *role = menu->addAction(subtitle);
    role->setEnabled(false);
    menu->addSeparator();

    menu->addAction(TR("My profile"), this, [this] {
        const Account fresh =
            system->accountManager()->findAccountByUsername(account.getUsername());
        ProfileDialog dialog(system, fresh.isValid() ? fresh : account, this);
        dialog.exec();
    });

    menu->addAction(TR("Change my password"), this, [this] {
        PasswordDialog dialog(system, account.getUsername(), this);
        dialog.exec();
    });

    menu->addSeparator();

    // ---- language, with a tick beside the active one
    auto *langMenu = menu->addMenu(TR("Language"));
    auto *langGroup = new QActionGroup(langMenu);
    langGroup->setExclusive(true);
    const bool isVi = Lang::instance()->language() == Lang::Vietnamese;

    QAction *en = langMenu->addAction("English");
    QAction *vi = langMenu->addAction(QString::fromUtf8("Ti\u1EBFng Vi\u1EC7t"));
    for (QAction *a : {en, vi}) {
        a->setCheckable(true);
        langGroup->addAction(a);
    }
    en->setChecked(!isVi);
    vi->setChecked(isVi);

    auto switchTo = [](Lang::Language next) {
        // Changing the language rebuilds the window, destroying this very menu;
        // queue it so the click finishes handling first.
        QMetaObject::invokeMethod(Lang::instance(), [next] {
            Lang::instance()->setLanguage(next);
        }, Qt::QueuedConnection);
    };
    connect(en, &QAction::triggered, this, [switchTo] { switchTo(Lang::English); });
    connect(vi, &QAction::triggered, this, [switchTo] { switchTo(Lang::Vietnamese); });

    // ---- theme, the same way
    auto *themeMenu = menu->addMenu(TR("Appearance"));
    auto *themeGroup = new QActionGroup(themeMenu);
    themeGroup->setExclusive(true);
    QAction *light = themeMenu->addAction(TR("Light"));
    QAction *dark = themeMenu->addAction(TR("Dark"));
    for (QAction *a : {light, dark}) {
        a->setCheckable(true);
        themeGroup->addAction(a);
    }
    light->setChecked(!Theme::isDark());
    dark->setChecked(Theme::isDark());

    auto setTheme = [this](Theme::Mode mode) {
        if (Theme::mode() == mode)
            return;
        QMetaObject::invokeMethod(this, [this, mode] {
            Theme::setMode(mode);
            setStyleSheet(Theme::appStyleSheet());
            // The pages read Theme::danger() and friends while filling their
            // tables, so they have to be rebuilt for new colours to appear.
            rebuildUi();
        }, Qt::QueuedConnection);
    };
    connect(light, &QAction::triggered, this, [setTheme] { setTheme(Theme::Light); });
    connect(dark, &QAction::triggered, this, [setTheme] { setTheme(Theme::Dark); });

    menu->addSeparator();

    menu->addAction(TR("Sign out"), this, [this] {
        const auto answer = QMessageBox::question(this, TR("Sign out"),
                                                  TR("Sign out of this account?"));
        if (answer != QMessageBox::Yes)
            return;
        logoutFlag = true;   // main() reads this and reopens the login dialog
        close();
    });

    return menu;
}

void MainWindow::buildNavEntries()
{
    navEntries.clear();
    if (account.isLibrarian()) {
        navEntries.append({TR("Dashboard"), TR("Today at a glance"),
                           [this] { return new DashboardPage(system, this); }});
        navEntries.append({TR("Books"), TR("Catalogue and stock"),
                           [this] { return new BooksPage(system, true, QString(), this); }});
        navEntries.append({TR("Readers"), TR("Members and their loans"),
                           [this] { return new ReadersPage(system, this); }});
        navEntries.append({TR("Borrow / Return"), TR("The desk"),
                           [this] { return new BorrowPage(system, this); }});
        navEntries.append({TR("Manage"), TR("Accounts, rules, activity"),
                           [this] {
                               return new AdminPage(system, account.getUsername(), this);
                           }});
    } else {
        // A patron sees only their own loans and a catalogue they can request
        // from — no reader list, no borrow history of other people.
        const QString me = account.getLinkedReaderID();
        navEntries.append({TR("My books"), TR("What I hold and owe"),
                           [this, me] { return new MyLoansPage(system, me, this); }});
        navEntries.append({TR("Browse catalogue"), TR("Find and request a book"),
                           [this, me] { return new BooksPage(system, false, me, this); }});
    }
}

void MainWindow::warnIfOverdue()
{
    const QString me = account.getLinkedReaderID();
    const QList<BorrowRecord> late = system->getOverdueRecordsByReader(me);
    if (late.isEmpty())
        return;

    const QDate today = QDate::currentDate();
    QStringList lines;
    for (const BorrowRecord &r : late) {
        const Book b = system->bookManager()->findBookByID(r.getBookID());
        lines << QString("• %1 — %2")
                     .arg(b.isValid() ? b.getTitle() : r.getBookID(),
                          QString(TR("due %1, %2 day(s) late"))
                              .arg(r.getDueDate()).arg(r.lateDays(today)));
    }

    QMessageBox::warning(
        this, TR("You have overdue books"),
        QString(TR("%1\n\nYou owe %2. You cannot request or borrow anything else "
                   "until these are returned."))
            .arg(lines.join("\n"), LibrarySystem::formatMoney(system->outstandingFine(me))));
}

bool MainWindow::logoutRequested() const
{
    return logoutFlag;
}
