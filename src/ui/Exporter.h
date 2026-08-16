#ifndef EXPORTER_H
#define EXPORTER_H

#include <QString>

class QTableWidget;
class QWidget;

// Writes whatever a table is currently showing to a CSV file the user picks.
// Exporting the widget rather than the database means the file matches exactly
// what is on screen, filters, sorting and all.
namespace Exporter {

// Returns false only on a real failure; a cancelled file dialog returns true
// with nothing written.
bool exportTableToCsv(QTableWidget *table, const QString &suggestedName, QWidget *parent);

} // namespace Exporter

#endif // EXPORTER_H
