#include "ui/Theme.h"

#include <QPainter>
#include <QPainterPath>
#include <QPixmap>
#include <QSettings>

namespace Theme {

// ------------------------------------------------------------------ mode

static Mode g_mode = Light;
static bool g_loaded = false;

Mode mode()
{
    if (!g_loaded) {
        g_mode = static_cast<Mode>(QSettings("HCMUS", "LibraryApp")
                                       .value("theme", Light).toInt());
        g_loaded = true;
    }
    return g_mode;
}

void setMode(Mode m)
{
    g_mode = m;
    g_loaded = true;
    QSettings("HCMUS", "LibraryApp").setValue("theme", static_cast<int>(m));
}

bool isDark() { return mode() == Dark; }

// --------------------------------------------------------------- palette
// The status colours are lightened in dark mode: the light-theme versions are
// too dark to read against a dark background.

QColor ink()      { return isDark() ? QColor("#e7ecf5") : QColor("#1b2439"); }
QColor body()     { return isDark() ? QColor("#c2cbdb") : QColor("#39435a"); }
QColor muted()    { return isDark() ? QColor("#8b96ad") : QColor("#78839a"); }
QColor primary()  { return isDark() ? QColor("#5b9bf5") : QColor("#2f6fd0"); }
QColor success()  { return isDark() ? QColor("#5fc98d") : QColor("#1e7a4b"); }
QColor warning()  { return isDark() ? QColor("#e0a95c") : QColor("#b3701a"); }
QColor danger()   { return isDark() ? QColor("#f0736a") : QColor("#c0392b"); }
QColor critical() { return isDark() ? QColor("#d982c8") : QColor("#7d1f6a"); }

// ------------------------------------------------------------ dark sheet

static QString darkOverrides()
{
    // Appended after the light rules; equal-specificity rules that come later
    // win, so only the colours that actually differ are repeated here.
    return QStringLiteral(R"(
        QWidget { color: #c2cbdb; }
        QMainWindow, QWidget#pageBody, QDialog { background: #11161f; }

        QLabel#pageHeading, QLabel#sectionTitle, QLabel#brandTitle,
        QLabel#dialogHeading { color: #e7ecf5; }
        QLabel#pageSub, QLabel#brandSub, QLabel#dialogSummary { color: #8b96ad; }
        QLabel#formError { color: #f0736a; }
        QLabel#hintBox { background: #16283f; border: 1px solid #244a72; color: #9dc4f5; }
        QLabel#warnBox { background: #33240f; border: 1px solid #5b421c; color: #e8b473; }
        QLabel#detailBox { background: #19202c; border: 1px solid #2a3446; color: #c2cbdb; }

        QLineEdit, QSpinBox, QDateEdit, QComboBox {
            background: #19202c; border: 1px solid #2f3a4d; color: #e7ecf5;
        }
        QLineEdit:read-only { background: #161c26; color: #8b96ad; }
        QLineEdit:focus, QSpinBox:focus, QDateEdit:focus, QComboBox:focus {
            border: 1px solid #5b9bf5;
        }

        QPushButton { background: #3d80e6; }
        QPushButton:hover { background: #5b9bf5; }
        QPushButton:disabled { background: #262f3e; color: #5c6779; }
        QPushButton#secondaryButton {
            background: #19202c; color: #7fb2f7; border: 1px solid #2f3a4d;
        }
        QPushButton#secondaryButton:hover { background: #223047; }
        QPushButton#secondaryButton:disabled {
            background: #161c26; color: #4d5666; border: 1px solid #232b38;
        }
        QPushButton#dangerButton { background: #b23a2e; }
        QPushButton#linkButton { color: #7fb2f7; }
        QPushButton#linkButton:hover { color: #e7ecf5; }

        QTableWidget {
            background: #161c26; alternate-background-color: #19202c;
            border: 1px solid #2a3446; color: #c2cbdb;
            selection-background-color: #24405f; selection-color: #ffffff;
        }
        QTableWidget::item { border-bottom: 1px solid #212a37; }
        QHeaderView::section {
            background: #1b2330; color: #96a2b8; border-bottom: 2px solid #2a3446;
        }
        QHeaderView::section:hover { background: #223047; color: #7fb2f7; }

        QGroupBox { background: #161c26; border: 1px solid #2a3446; color: #e7ecf5; }

        QTabWidget::pane { background: #11161f; }
        QTabBar::tab { color: #8b96ad; }
        QTabBar::tab:hover { background: #1b2330; color: #7fb2f7; }
        QTabBar::tab:selected { background: #19202c; color: #e7ecf5; border: 1px solid #2a3446; }

        QWidget#sidebar { background: #161c26; border-right: 1px solid #2a3446; }
        QPushButton#navButton { color: #96a2b8; }
        QPushButton#navButton:hover { background: #1f2a3a; color: #7fb2f7; }
        QPushButton#navButton:checked { background: #2f6fd0; color: #ffffff; }
        QLabel#sidebarFoot { color: #6d7a92; }
        QLabel#sidebarFootStrong { color: #c2cbdb; }

        QToolButton#avatarButton:hover { background: #1f2a3a; border: 1px solid #2f3a4d; }
        QMenu, QMenu#accountMenu { background: #19202c; border: 1px solid #2a3446; }
        QMenu::item, QMenu#accountMenu::item { color: #c2cbdb; }
        QMenu::item:selected, QMenu#accountMenu::item:selected {
            background: #223047; color: #7fb2f7;
        }
        QMenu#accountMenu::item:disabled { color: #6d7a92; }
        QMenu#accountMenu::separator { background: #2a3446; }

        QFrame#statCard { background: #19202c; border: 1px solid #2a3446; }
        QLabel#statNumber { color: #e7ecf5; }
        QLabel#statTitle  { color: #7986a0; }

        QScrollBar::handle:vertical, QScrollBar::handle:horizontal { background: #333f52; }
        QScrollBar::handle:vertical:hover, QScrollBar::handle:horizontal:hover { background: #45536b; }
    )");
}

static QString common()
{
    return QStringLiteral(R"(
        QWidget { color: #39435a; font-size: 10pt; }

        /* ---------------------------------------------------------- text */
        QLabel#pageHeading   { font-size: 13pt; font-weight: bold; color: #1b2439; }
        QLabel#pageSub       { color: #78839a; }
        QLabel#sectionTitle  { font-size: 10pt; font-weight: bold; color: #1b2439; }
        QLabel#formError     { color: #c0392b; font-weight: bold; }
        QLabel#hintBox {
            background: #eaf1fc; border: 1px solid #c6dbf6; border-radius: 8px;
            padding: 9px 11px; color: #23528f;
        }
        QLabel#warnBox {
            background: #fdf1e8; border: 1px solid #f0cdb0; border-radius: 8px;
            padding: 9px 11px; color: #97541a;
        }
        QLabel#detailBox {
            background: #f8fafd; border: 1px solid #e3e8ef; border-radius: 8px;
            padding: 11px 13px; color: #39435a; line-height: 150%;
        }

        /* --------------------------------------------------------- input */
        QLineEdit, QSpinBox, QDateEdit, QComboBox {
            border: 1px solid #dfe4ec; border-radius: 7px;
            padding: 7px 9px; background: #ffffff; selection-background-color: #cfe0f7;
        }
        QLineEdit:focus, QSpinBox:focus, QDateEdit:focus, QComboBox:focus {
            border: 1px solid #2f6fd0;
        }
        QLineEdit:read-only { background: #f2f4f8; color: #78839a; }
        QComboBox::drop-down { border: none; width: 22px; }

        /* -------------------------------------------------------- buttons */
        QPushButton {
            background: #2f6fd0; color: #ffffff; border: none; border-radius: 7px;
            padding: 8px 16px; font-weight: bold;
        }
        QPushButton:hover   { background: #3d80e6; }
        QPushButton:pressed { background: #2960b6; }
        QPushButton:disabled { background: #d5dbe6; color: #98a2b3; }

        QPushButton#secondaryButton {
            background: #ffffff; color: #2f6fd0; border: 1px solid #c6d4e8;
        }
        QPushButton#secondaryButton:hover    { background: #eef4fd; }
        QPushButton#secondaryButton:disabled { background: #f4f6fa; color: #b3bccb;
                                               border: 1px solid #e3e8ef; }

        QPushButton#dangerButton { background: #c0392b; }
        QPushButton#dangerButton:hover { background: #d4503f; }

        QPushButton#linkButton {
            background: transparent; color: #2f6fd0; font-weight: normal; padding: 3px;
        }
        QPushButton#linkButton:hover { color: #1b2439; text-decoration: underline; }

        /* --------------------------------------------------------- tables */
        QTableWidget {
            background: #ffffff; alternate-background-color: #fafbfd;
            border: 1px solid #e3e8ef; border-radius: 8px;
            gridline-color: transparent;
            selection-background-color: #dbe9fb; selection-color: #1b2439;
        }
        QTableWidget::item { padding: 7px 4px; border-bottom: 1px solid #eef1f6; }
        QTableWidget::item:selected { border-bottom: 1px solid #cadcf5; }
        QHeaderView::section {
            background: #f2f5fa; color: #55617a; padding: 9px 6px;
            border: none; border-bottom: 2px solid #e3e8ef; font-weight: bold;
        }
        QHeaderView::section:hover { background: #e8eef8; color: #2f6fd0; }

        /* --------------------------------------------------------- groups */
        QGroupBox {
            border: 1px solid #e3e8ef; border-radius: 10px; background: #ffffff;
            margin-top: 15px; padding: 12px 10px 10px 10px;
            font-weight: bold; color: #1b2439;
        }
        QGroupBox::title {
            subcontrol-origin: margin; left: 12px; padding: 0 6px; background: transparent;
        }

        QScrollArea { background: transparent; border: none; }
        QScrollArea > QWidget > QWidget { background: transparent; }

        QScrollBar:vertical { background: transparent; width: 12px; margin: 2px; }
        QScrollBar::handle:vertical { background: #ccd4e2; border-radius: 6px; min-height: 34px; }
        QScrollBar::handle:vertical:hover { background: #adb9cd; }
        QScrollBar:horizontal { background: transparent; height: 12px; margin: 2px; }
        QScrollBar::handle:horizontal { background: #ccd4e2; border-radius: 6px; min-width: 34px; }
        QScrollBar::handle:horizontal:hover { background: #adb9cd; }
        QScrollBar::add-line, QScrollBar::sub-line { height: 0; width: 0; }
        QScrollBar::add-page, QScrollBar::sub-page { background: transparent; }
    )");
}

QString appStyleSheet()
{
    const QString sheet = common() + QStringLiteral(R"(
        QMainWindow, QWidget#pageBody { background: #f4f6fa; }

        /* ----------------------------------------------------- app header */
        QWidget#appHeader { background: #1b2439; }
        QLabel#appTitle    { color: #ffffff; font-size: 11pt; font-weight: bold; }
        QLabel#appUser     { color: #9fb2d4; font-weight: bold; }
        QLabel#appUserSub  { color: #6d80a3; }
        QWidget#appHeader QComboBox {
            background: #2b3853; color: #dbe4f5; border: 1px solid #3a4a6b;
            border-radius: 6px; padding: 4px 8px; font-weight: bold;
        }
        QWidget#appHeader QComboBox QAbstractItemView {
            background: #ffffff; color: #39435a; selection-background-color: #dbe9fb;
        }
        QPushButton#logoutButton {
            background: transparent; color: #dbe4f5;
            border: 1px solid #46557a; padding: 5px 14px;
        }
        QPushButton#logoutButton:hover { background: #2b3853; color: #ffffff; }

        /* ------------------------------------------------- avatar button */
        QToolButton#avatarButton {
            background: transparent; border: 1px solid transparent;
            border-radius: 22px; padding: 3px 8px 3px 3px;
            color: #9fb2d4; font-size: 9pt;
        }
        QToolButton#avatarButton:hover {
            background: #2b3853; border: 1px solid #3a4a6b; color: #ffffff;
        }
        QToolButton#avatarButton::menu-indicator { image: none; width: 0; }

        QMenu#accountMenu {
            background: #ffffff; border: 1px solid #e3e8ef; border-radius: 10px;
            padding: 6px;
        }
        QMenu#accountMenu::item {
            padding: 9px 30px 9px 16px; border-radius: 7px; color: #39435a;
        }
        QMenu#accountMenu::item:selected { background: #eef4fd; color: #2f6fd0; }
        QMenu#accountMenu::item:disabled { color: #8a94a8; }
        QMenu#accountMenu::separator {
            height: 1px; background: #e3e8ef; margin: 6px 10px;
        }
        QMenu { background: #ffffff; border: 1px solid #e3e8ef; padding: 5px; }
        QMenu::item { padding: 8px 26px 8px 15px; border-radius: 6px; }
        QMenu::item:selected { background: #eef4fd; color: #2f6fd0; }

        /* ------------------------------------------------ language switch */
        QWidget#langSwitch {
            background: #2b3853; border: 1px solid #3a4a6b; border-radius: 8px;
        }
        QPushButton#langOption {
            background: transparent; color: #93a5c6;
            border: none; border-radius: 6px;
            padding: 4px 0; font-weight: bold; font-size: 9pt;
        }
        QPushButton#langOption:hover { color: #ffffff; }
        QPushButton#langOption:checked { background: #3d80e6; color: #ffffff; }

        /* ------------------------------------------------------- tab strip */
        QTabWidget::pane { border: none; background: #f4f6fa; }
        QTabBar { qproperty-drawBase: 0; }
        QTabBar::tab {
            padding: 9px 20px; margin: 6px 4px 0 0;
            background: transparent; color: #6b7790;
            border: none; border-radius: 8px; font-weight: bold;
        }
        QTabBar::tab:hover    { background: #e7edf7; color: #2f6fd0; }
        QTabBar::tab:selected { background: #ffffff; color: #1b2439;
                                border: 1px solid #e3e8ef; }

        /* -------------------------------------------------------- sidebar */
        QWidget#sidebar { background: #ffffff; border-right: 1px solid #e3e8ef; }
        QPushButton#navButton {
            background: transparent; color: #55617a;
            border: none; border-radius: 9px;
            padding: 11px 13px; text-align: left; font-weight: bold;
        }
        QPushButton#navButton:hover { background: #eef4fd; color: #2f6fd0; }
        QPushButton#navButton:checked { background: #2f6fd0; color: #ffffff; }
        QLabel#sidebarFoot { color: #98a2b3; font-size: 8pt; }
        QLabel#sidebarFootStrong { color: #55617a; font-weight: bold; }

        /* ------------------------------------------------------ stat cards */
        QFrame#statCard {
            background: #ffffff; border: 1px solid #e3e8ef; border-radius: 12px;
        }
        QLabel#statNumber { font-size: 20pt; font-weight: bold; color: #1b2439; }
        QLabel#statTitle  { font-size: 8pt; font-weight: bold; color: #8a94a8;
                            letter-spacing: 1px; }
    )");
    return isDark() ? sheet + darkOverrides() : sheet;
}

QString dialogStyleSheet()
{
    const QString sheet = common() + QStringLiteral(R"(
        QDialog { background: #f4f6fa; }

        QLabel#brandTitle { font-size: 16pt; font-weight: bold; color: #1b2439; }
        QLabel#brandSub   { color: #78839a; }
        QLabel#dialogHeading { font-size: 10pt; font-weight: bold; color: #1b2439; }
        QLabel#dialogSummary { color: #78839a; }

        QTabWidget::pane {
            border: 1px solid #e3e8ef; border-radius: 10px;
            background: #ffffff; top: -1px;
        }
        QTabBar::tab {
            padding: 9px 22px; margin-right: 4px;
            background: #e9edf5; color: #6b7790;
            border: 1px solid #e3e8ef; border-bottom: none;
            border-top-left-radius: 8px; border-top-right-radius: 8px;
            font-weight: bold;
        }
        QTabBar::tab:selected { background: #ffffff; color: #1b2439; }
        QTabBar::tab:hover:!selected { color: #2f6fd0; }
    )");
    return isDark() ? sheet + darkOverrides() : sheet;
}

QIcon eyeIcon(bool open)
{
    const int size = 20;
    QPixmap pm(size, size);
    pm.fill(Qt::transparent);

    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    QPen pen(muted());
    pen.setWidthF(1.6);
    pen.setCapStyle(Qt::RoundCap);
    p.setPen(pen);
    p.setBrush(Qt::NoBrush);

    // The almond outline: two arcs meeting at the corners.
    QPainterPath almond;
    almond.moveTo(2.5, 10);
    almond.quadTo(10, 3, 17.5, 10);
    almond.quadTo(10, 17, 2.5, 10);
    p.drawPath(almond);

    // Pupil.
    p.setBrush(muted());
    p.drawEllipse(QPointF(10, 10), 2.6, 2.6);

    if (!open) {
        // A slash means "hidden"; the eye alone would not say which state it is.
        p.setBrush(Qt::NoBrush);
        p.drawLine(QPointF(4, 16), QPointF(16, 4));
    }
    p.end();
    return QIcon(pm);
}

QString pill(const QString &text, const QColor &colour)
{
    // A coloured label rather than a real widget, so it can be dropped into
    // any QLabel that accepts rich text.
    return QString("<span style='color:%1; font-weight:bold;'>%2</span>")
        .arg(colour.name(), text.toHtmlEscaped());
}

} // namespace Theme
