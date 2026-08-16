#ifndef READERDETAILDIALOG_H
#define READERDETAILDIALOG_H

#include <QDialog>

#include "managers/LibrarySystem.h"
#include "models/Reader.h"

class QLabel;
class QTableWidget;

// The full profile of one reader in its own window: picture, contact details,
// how many books they hold, what they owe, and the loans themselves.
// Opened from the readers list by a librarian.
class ReaderDetailDialog : public QDialog
{
    Q_OBJECT

public:
    ReaderDetailDialog(LibrarySystem *system, const QString &readerID,
                       bool allowPhotoChange, QWidget *parent = nullptr);

private slots:
    void refresh();
    void onChangePhoto();
    void onRemovePhoto();

private:
    QWidget *buildHeader();

    LibrarySystem *system;
    QString readerID;
    bool allowPhotoChange;

    QLabel *photoLabel;
    QLabel *nameLabel;
    QLabel *idLabel;
    QLabel *contactLabel;
    QLabel *statsLabel;
    QTableWidget *loansTable;
};

#endif // READERDETAILDIALOG_H
