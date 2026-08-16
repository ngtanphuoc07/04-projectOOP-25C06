#ifndef RECORDSDIALOG_H
#define RECORDSDIALOG_H

#include <QDialog>
#include <QList>

#include "managers/LibrarySystem.h"
#include "models/BorrowRecord.h"

class QComboBox;
class QLabel;
class QTableWidget;

// Reusable read-only viewer for a set of borrow records. Used for
//   * the full borrow history of one book      (BooksPage)
//   * everything one reader has ever borrowed  (ReadersPage)
// The two callers differ only in the title, the summary line and the records
// they pass in, so one dialog serves both.
class RecordsDialog : public QDialog
{
    Q_OBJECT

public:
    RecordsDialog(LibrarySystem *system, const QString &title, const QString &subtitle,
                  const QList<BorrowRecord> &records, QWidget *parent = nullptr);

private slots:
    void fillTable();

private:
    LibrarySystem *system;
    QList<BorrowRecord> allRecords;

    QComboBox *statusFilter;
    QTableWidget *table;
    QLabel *summaryLabel;
};

#endif // RECORDSDIALOG_H
