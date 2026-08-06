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
    QList<Reader> searchReadersByName(const QString &keyword);
    Reader findReaderByID(const QString &readerID);

private:
    DatabaseManager *db;
};

#endif // READERMANAGER_H
