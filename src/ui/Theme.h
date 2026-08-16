#ifndef THEME_H
#define THEME_H

#include <QColor>
#include <QIcon>
#include <QString>

// One place for every colour and every rule of the look, so the windows and
// the dialogs cannot drift apart visually.
namespace Theme {

// Light or dark. Stored in QSettings and applied to every window.
enum Mode { Light = 0, Dark = 1 };
Mode mode();
void setMode(Mode mode);
bool isDark();

// Palette
QColor ink();       // headings
QColor body();      // normal text
QColor muted();     // secondary text
QColor primary();   // accent / links
QColor success();   // returned, on time
QColor warning();   // still out
QColor danger();    // overdue
QColor critical();  // not returned / lost

// Whole-window stylesheet (main window and its pages).
QString appStyleSheet();

// Dialog stylesheet: same language, tuned for a small floating window.
QString dialogStyleSheet();

// An eye, drawn rather than shipped as a file so it always matches the theme
// colours and needs no resource system. open=false adds the slash through it.
QIcon eyeIcon(bool open);

// Small coloured "pill" markup used inside table cells and labels.
QString pill(const QString &text, const QColor &colour);

} // namespace Theme

#endif // THEME_H
