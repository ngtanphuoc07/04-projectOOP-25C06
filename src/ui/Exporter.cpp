#include "ui/Exporter.h"

#include <QDate>
#include <QFile>
#include <QFileDialog>
#include <QHeaderView>
#include <QMessageBox>
#include <QTableWidget>
#include <QTextStream>

#include "i18n/Lang.h"

namespace {

// Wraps a field in quotes when it contains anything that would break the file.
QString csvField(const QString &value)
{
    QString out = value;
    if (out.contains('"') || out.contains(',') || out.contains('\n') || out.contains('\r')) {
        out.replace('"', "\"\"");
        return '"' + out + '"';
    }
    return out;
}

} // namespace

namespace Exporter {

bool exportTableToCsv(QTableWidget *table, const QString &suggestedName, QWidget *parent)
{
    if (!table || table->rowCount() == 0) {
        QMessageBox::information(parent, TR("Export"), TR("There is nothing to export."));
        return true;
    }

    const QString suggestion = QString("%1-%2.csv")
                                   .arg(suggestedName,
                                        QDate::currentDate().toString("yyyy-MM-dd"));
    const QString path = QFileDialog::getSaveFileName(
        parent, TR("Export to CSV"), suggestion, TR("CSV files (*.csv)"));
    if (path.isEmpty())
        return true; // the user changed their mind

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(parent, TR("Export"),
                             QString(TR("Could not write to \"%1\".")).arg(path));
        return false;
    }

    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    // Excel needs the byte order mark to recognise UTF-8, otherwise Vietnamese
    // names open as mojibake.
    out << QChar(0xFEFF);

    QStringList header;
    for (int c = 0; c < table->columnCount(); ++c) {
        if (table->isColumnHidden(c))
            continue;
        header << csvField(table->horizontalHeaderItem(c)
                               ? table->horizontalHeaderItem(c)->text()
                               : QString());
    }
    out << header.join(',') << '\n';

    // Visual order, so the file matches the sorting the user chose.
    for (int visual = 0; visual < table->rowCount(); ++visual) {
        const int row = table->verticalHeader()->logicalIndex(visual);
        QStringList line;
        for (int c = 0; c < table->columnCount(); ++c) {
            if (table->isColumnHidden(c))
                continue;
            const QTableWidgetItem *item = table->item(row, c);
            line << csvField(item ? item->text() : QString());
        }
        out << line.join(',') << '\n';
    }

    file.close();
    QMessageBox::information(parent, TR("Export"),
                             QString(TR("Saved %1 row(s) to:\n%2"))
                                 .arg(table->rowCount()).arg(path));
    return true;
}

} // namespace Exporter
