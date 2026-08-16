#ifndef READERMANAGER_H
#define READERMANAGER_H

#include <QList>

#include "database/DatabaseManager.h"
#include "models/Reader.h"

// Business rules for readers. Sits between the GUI and DatabaseManager.
class ReaderManager
{
public:
    explicit ReaderManager(DatabaseManager *db);

    bool addReader(const Reader &reader, QString *errorOut = nullptr);
    bool updateReader(const Reader &reader, QString *errorOut = nullptr);
    bool deleteReader(const QString &readerID, QString *errorOut = nullptr);

    QList<Reader> getAllReaders();
    // Matches the keyword against ID, full name, phone and email.
    QList<Reader> searchReaders(const QString &keyword);
    Reader findReaderByID(const QString &readerID);

    // Next free sequential ID (R001, R002, ...).
    QString nextReaderID();

    // The avatar is owned by the profile screen, not the CRUD form, so it gets
    // its own setter — exactly like borrowedCount belongs to the loan workflow.
    bool updateAvatar(const QString &readerID, const QByteArray &png,
                      QString *errorOut = nullptr);

    // Digits only, 9-11 of them. Spaces, dots, dashes and a leading +84 are
    // accepted from the user and stripped here, so a perfectly normal way of
    // writing a phone number is not rejected.
    static QString normalisePhone(const QString &raw);

private:
    DatabaseManager *db;
};

#endif // READERMANAGER_H
