#ifndef SLIPDIALOG_H
#define SLIPDIALOG_H

#include <QDialog>

#include "managers/LibrarySystem.h"

class QTextBrowser;

// The reader's slip: what they have requested, what they are holding, when it
// is due and what they owe. Printable and savable, so it can be handed over at
// the desk instead of the reader creating loans themselves.
class SlipDialog : public QDialog
{
    Q_OBJECT

public:
    SlipDialog(LibrarySystem *system, const QString &readerID, QWidget *parent = nullptr);

private slots:
    void onSave();
    void onPrint();

private:
    QString buildHtml() const;

    LibrarySystem *system;
    QString readerID;
    QTextBrowser *view;
};

#endif // SLIPDIALOG_H
