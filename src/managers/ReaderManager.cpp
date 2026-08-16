#include "managers/ReaderManager.h"
#include "i18n/Lang.h"
#include "util/SearchUtil.h"

#include <algorithm>

ReaderManager::ReaderManager(DatabaseManager *db)
    : db(db)
{
}

bool ReaderManager::addReader(const Reader &reader, QString *errorOut)
{
    if (db->findReaderByID(reader.getReaderID()).isValid()) {
        if (errorOut)
            *errorOut = QString(TR("A reader with ID \"%1\" already exists.")).arg(reader.getReaderID());
        return false;
    }
    if (!db->insertReader(reader)) {
        if (errorOut)
            *errorOut = TR("Database error while inserting the reader.");
        return false;
    }
    return true;
}

bool ReaderManager::updateReader(const Reader &reader, QString *errorOut)
{
    Reader existing = db->findReaderByID(reader.getReaderID());
    if (!existing.isValid()) {
        if (errorOut)
            *errorOut = QString(TR("Reader \"%1\" was not found.")).arg(reader.getReaderID());
        return false;
    }

    // borrowedCount belongs to the borrow/return workflow and the avatar to the
    // profile screen; the edit form owns neither, so keep the stored values.
    Reader updated = reader;
    updated.setBorrowedCount(existing.getBorrowedCount());
    updated.setAvatar(existing.getAvatar());
    if (!db->updateReader(updated)) {
        if (errorOut)
            *errorOut = TR("Database error while updating the reader.");
        return false;
    }
    return true;
}

bool ReaderManager::deleteReader(const QString &readerID, QString *errorOut)
{
    Reader existing = db->findReaderByID(readerID);
    if (!existing.isValid()) {
        if (errorOut)
            *errorOut = QString(TR("Reader \"%1\" was not found.")).arg(readerID);
        return false;
    }
    if (existing.getBorrowedCount() > 0) {
        if (errorOut)
            *errorOut = QString(TR("Reader \"%1\" still has %2 borrowed book(s) and cannot be deleted."))
                            .arg(readerID)
                            .arg(existing.getBorrowedCount());
        return false;
    }
    if (!db->deleteReader(readerID)) {
        if (errorOut)
            *errorOut = TR("Database error while deleting the reader.");
        return false;
    }
    return true;
}

QList<Reader> ReaderManager::getAllReaders()
{
    return db->getAllReaders();
}

QList<Reader> ReaderManager::searchReaders(const QString &keyword)
{
    QList<QPair<int, Reader>> scored;
    const QList<Reader> all = db->getAllReaders();
    for (const Reader &r : all) {
        const int s = SearchUtil::scoreAny(
            {r.getFullName(), r.getReaderID(), r.getPhone(), r.getEmail()}, keyword);
        if (s > 0)
            scored.append({s, r});
    }
    std::stable_sort(scored.begin(), scored.end(),
                     [](const QPair<int, Reader> &a, const QPair<int, Reader> &b) {
                         return a.first > b.first;
                     });

    QList<Reader> result;
    result.reserve(scored.size());
    for (const auto &pair : scored)
        result.append(pair.second);
    return result;
}

bool ReaderManager::updateAvatar(const QString &readerID, const QByteArray &png,
                                 QString *errorOut)
{
    Reader reader = db->findReaderByID(readerID);
    if (!reader.isValid()) {
        if (errorOut)
            *errorOut = QString(TR("Reader \"%1\" was not found.")).arg(readerID);
        return false;
    }
    reader.setAvatar(png);
    if (!db->updateReader(reader)) {
        if (errorOut)
            *errorOut = TR("Database error while saving the profile picture.");
        return false;
    }
    return true;
}

QString ReaderManager::nextReaderID()
{
    int maxNumber = 0;
    const QList<Reader> readers = db->getAllReaders();
    for (const Reader &r : readers) {
        const QString id = r.getReaderID();
        if (id.startsWith("R", Qt::CaseInsensitive)) {
            bool ok = false;
            const int n = id.mid(1).toInt(&ok);
            if (ok && n > maxNumber)
                maxNumber = n;
        }
    }
    return QString("R%1").arg(maxNumber + 1, 3, 10, QChar('0'));
}

QString ReaderManager::normalisePhone(const QString &raw)
{
    QString digits;
    for (const QChar &c : raw) {
        if (c.isDigit())
            digits.append(c);
    }
    // "+84 901 234 567" and "0901234567" are the same number.
    if (digits.startsWith("84") && digits.length() > 9)
        digits = "0" + digits.mid(2);
    return digits;
}

Reader ReaderManager::findReaderByID(const QString &readerID)
{
    return db->findReaderByID(readerID);
}
