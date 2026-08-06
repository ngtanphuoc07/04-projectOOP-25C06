#include "managers/ReaderManager.h"

ReaderManager::ReaderManager(DatabaseManager *db)
    : db(db)
{
}

bool ReaderManager::addReader(const Reader &reader, QString *errorOut)
{
    if (db->findReaderByID(reader.getReaderID()).isValid()) {
        if (errorOut)
            *errorOut = QString("A reader with ID \"%1\" already exists.").arg(reader.getReaderID());
        return false;
    }
    if (!db->insertReader(reader)) {
        if (errorOut)
            *errorOut = "Database error while inserting the reader.";
        return false;
    }
    return true;
}

bool ReaderManager::updateReader(const Reader &reader, QString *errorOut)
{
    Reader existing = db->findReaderByID(reader.getReaderID());
    if (!existing.isValid()) {
        if (errorOut)
            *errorOut = QString("Reader \"%1\" was not found.").arg(reader.getReaderID());
        return false;
    }

    // borrowedCount is owned by the borrow/return workflow, keep the stored value.
    Reader updated = reader;
    updated.setBorrowedCount(existing.getBorrowedCount());
    if (!db->updateReader(updated)) {
        if (errorOut)
            *errorOut = "Database error while updating the reader.";
        return false;
    }
    return true;
}

bool ReaderManager::deleteReader(const QString &readerID, QString *errorOut)
{
    Reader existing = db->findReaderByID(readerID);
    if (!existing.isValid()) {
        if (errorOut)
            *errorOut = QString("Reader \"%1\" was not found.").arg(readerID);
        return false;
    }
    if (existing.getBorrowedCount() > 0) {
        if (errorOut)
            *errorOut = QString("Reader \"%1\" still has %2 borrowed book(s) and cannot be deleted.")
                            .arg(readerID)
                            .arg(existing.getBorrowedCount());
        return false;
    }
    if (!db->deleteReader(readerID)) {
        if (errorOut)
            *errorOut = "Database error while deleting the reader.";
        return false;
    }
    return true;
}

QList<Reader> ReaderManager::getAllReaders()
{
    return db->getAllReaders();
}

QList<Reader> ReaderManager::searchReadersByName(const QString &keyword)
{
    QList<Reader> result;
    const QList<Reader> all = db->getAllReaders();
    for (const Reader &r : all) {
        if (r.getFullName().contains(keyword, Qt::CaseInsensitive))
            result.append(r);
    }
    return result;
}

Reader ReaderManager::findReaderByID(const QString &readerID)
{
    return db->findReaderByID(readerID);
}
