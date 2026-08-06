#include <QApplication>
#include <QMessageBox>
#include <QStyleFactory>

#include "database/DatabaseManager.h"
#include "managers/LibrarySystem.h"
#include "ui/MainWindow.h"

// The app is styled for a light background. Without this, Windows dark mode
// swaps Qt's default palette (dark table base, white label text) underneath
// the stylesheet and half the text becomes invisible — so pin one identical
// light look on every machine.
static void applyLightTheme(QApplication &app)
{
    app.setStyle(QStyleFactory::create("Fusion"));

    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#eef1f6"));
    palette.setColor(QPalette::WindowText, QColor("#2f3b52"));
    palette.setColor(QPalette::Base, Qt::white);
    palette.setColor(QPalette::AlternateBase, QColor("#f7f9fc"));
    palette.setColor(QPalette::ToolTipBase, Qt::white);
    palette.setColor(QPalette::ToolTipText, QColor("#2f3b52"));
    palette.setColor(QPalette::Text, QColor("#2f3b52"));
    palette.setColor(QPalette::Button, QColor("#eef1f6"));
    palette.setColor(QPalette::ButtonText, QColor("#2f3b52"));
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::Highlight, QColor("#cfe0f7"));
    palette.setColor(QPalette::HighlightedText, QColor("#17233a"));
    palette.setColor(QPalette::PlaceholderText, QColor("#8a97ab"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#9aa5b5"));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#9aa5b5"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#9aa5b5"));
    app.setPalette(palette);
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    applyLightTheme(app);

    DatabaseManager *db = DatabaseManager::instance();
    if (!db->connectDatabase() || !db->createTables()) {
        QMessageBox::critical(nullptr, "Library Management System",
                              "Could not open or initialise the SQLite database.\n"
                              "The application will now close.");
        return 1;
    }

    LibrarySystem system;
    system.seedSampleDataIfEmpty();

    MainWindow window(&system);
    window.show();

    return app.exec();
}
