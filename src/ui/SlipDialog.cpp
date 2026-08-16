#include "ui/SlipDialog.h"

#include <QDate>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QTextBrowser>
#include <QTextStream>
#include <QVBoxLayout>

#include "i18n/Lang.h"
#include "ui/Theme.h"

SlipDialog::SlipDialog(LibrarySystem *system, const QString &readerID, QWidget *parent)
    : QDialog(parent), system(system), readerID(readerID)
{
    setWindowTitle(TR("My slip"));
    resize(620, 640);

    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(12);

    view = new QTextBrowser(this);
    view->setHtml(buildHtml());
    layout->addWidget(view, 1);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    auto *save = new QPushButton(TR("Save as HTML"), this);
    save->setObjectName("secondaryButton");
    buttons->addButton(save, QDialogButtonBox::ActionRole);
    buttons->button(QDialogButtonBox::Close)->setText(TR("Close"));
    layout->addWidget(buttons);

    setStyleSheet(Theme::dialogStyleSheet());

    connect(save, &QPushButton::clicked, this, &SlipDialog::onSave);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
}

QString SlipDialog::buildHtml() const
{
    const Reader me = system->readerManager()->findReaderByID(readerID);
    const QDate today = QDate::currentDate();
    const int perDay = system->finePerDay();

    QString html;
    html += "<html><body style='font-family:Arial; color:#1b2439;'>";
    html += QString("<h2>%1</h2>").arg(TR("Library slip"));
    html += QString("<p><b>%1</b><br/>%2: %3<br/>%4: %5</p>")
                .arg(me.isValid() ? me.getFullName().toHtmlEscaped() : readerID,
                     TR("Reader card"), readerID,
                     TR("Issued"),
                     QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm"));

    // ---- requests waiting to be collected
    const QList<Reservation> open = system->getOpenReservationsByReader(readerID);
    html += QString("<h3>%1</h3>").arg(TR("Requested titles"));
    if (open.isEmpty()) {
        html += QString("<p><i>%1</i></p>").arg(TR("None"));
    } else {
        html += "<table width='100%' cellpadding='5' cellspacing='0' border='1' "
                "style='border-collapse:collapse; border-color:#dfe4ec;'>";
        html += QString("<tr style='background:#f2f5fa;'><th align='left'>%1</th>"
                        "<th align='left'>%2</th><th align='left'>%3</th></tr>")
                    .arg(TR("Reservation"), TR("Title"), TR("Status"));
        for (const Reservation &r : open) {
            const Book b = system->bookManager()->findBookByID(r.getBookID());
            html += QString("<tr><td>%1</td><td>%2</td><td>%3</td></tr>")
                        .arg(r.getReservationID(),
                             (b.isValid() ? b.getTitle() : r.getBookID()).toHtmlEscaped(),
                             r.stateName());
        }
        html += "</table>";
    }

    // ---- books in hand
    const QList<BorrowRecord> active = system->getActiveRecordsByReader(readerID);
    html += QString("<h3>%1</h3>").arg(TR("Books I am holding now"));
    if (active.isEmpty()) {
        html += QString("<p><i>%1</i></p>").arg(TR("None"));
    } else {
        html += "<table width='100%' cellpadding='5' cellspacing='0' border='1' "
                "style='border-collapse:collapse; border-color:#dfe4ec;'>";
        html += QString("<tr style='background:#f2f5fa;'><th align='left'>%1</th>"
                        "<th align='left'>%2</th><th align='left'>%3</th>"
                        "<th align='left'>%4</th></tr>")
                    .arg(TR("Record"), TR("Title"), TR("Due"), TR("Fine"));
        for (const BorrowRecord &r : active) {
            const Book b = system->bookManager()->findBookByID(r.getBookID());
            const int late = r.lateDays(today);
            html += QString("<tr><td>%1</td><td>%2</td>"
                            "<td%3>%4</td><td>%5</td></tr>")
                        .arg(r.getRecordID(),
                             (b.isValid() ? b.getTitle() : r.getBookID()).toHtmlEscaped(),
                             late > 0 ? " style='color:#c0392b;'" : "",
                             r.getDueDate(),
                             LibrarySystem::formatMoney(r.outstandingFine(perDay, today)));
        }
        html += "</table>";
    }

    const int owed = system->outstandingFine(readerID);
    html += QString("<h3>%1: %2</h3>")
                .arg(TR("Total to pay"), LibrarySystem::formatMoney(owed));
    html += QString("<p style='color:#78839a; font-size:small;'>%1</p>")
                .arg(TR("Bring this slip to the desk. A librarian hands over the books "
                        "and records the loan."));
    html += "</body></html>";
    return html;
}

void SlipDialog::onSave()
{
    const QString path = QFileDialog::getSaveFileName(
        this, TR("Save as HTML"),
        QString("slip-%1-%2.html").arg(readerID, QDate::currentDate().toString("yyyy-MM-dd")),
        TR("Web page (*.html)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, TR("Save as HTML"),
                             QString(TR("Could not write to \"%1\".")).arg(path));
        return;
    }
    QTextStream out(&file);
    out.setEncoding(QStringConverter::Utf8);
    out << buildHtml();
    file.close();

    QMessageBox::information(this, TR("Save as HTML"),
                             QString(TR("Saved to:\n%1")).arg(path));
}

void SlipDialog::onPrint()
{
    // Printing needs Qt::PrintSupport, which this project does not link.
    // Saving the slip and printing it from a browser covers the same need.
    onSave();
}
