#include "db.h"
#include "sql.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlRecord>
#include <QString>
#include <QStringList>
#include <QVariant>
#include <cstdlib>

QSqlQuery initDb(QSqlDatabase db)   {
    if(!db.open()) {
        qCritical() << "unable to open db";
        qDebug() << db.lastError().text();
        exit(-2);
    }   else    {
        QSqlQuery query = QSqlQuery(db);
        if(!db.tables().contains("note"))  {
            qInfo() << "table note is not present, creating";
            if(!(query.exec(QString(CREATE_NOTEBOOK_TABLE))
                    &&
                 query.exec(QString(CREATE_NOTE_TABLE))
                 ))
            {
                qCritical()  << "error creating database";
                qDebug() << query.lastQuery() << query.lastError() << db.lastError();
                exit(-1);
            }   else    {
                qInfo()  << "inserting default notebook";
                query.exec(QString(INIT_NOTEBOOK_TABLE));

            }
        }
        bool hasUuid = false;
        if(query.exec("PRAGMA table_info(note)")) {
            while(query.next()) {
                if(query.value(1).toString() == "uuid") { hasUuid = true; break; }
            }
        }
        if(!hasUuid) query.exec("ALTER TABLE note ADD COLUMN uuid TEXT");

        if(!db.tables().contains("attachment"))
            query.exec(CREATE_ATTACHMENT_TABLE);

        query.exec("CREATE TABLE IF NOT EXISTS note_tag(note INTEGER NOT NULL, tag TEXT NOT NULL COLLATE NOCASE, PRIMARY KEY(note, tag), FOREIGN KEY(note) REFERENCES note(id))");
        query.exec("CREATE INDEX IF NOT EXISTS note_tag_tag ON note_tag(tag)");
        query.exec("CREATE TRIGGER IF NOT EXISTS note_tag_ad AFTER DELETE ON note BEGIN DELETE FROM note_tag WHERE note = old.id; END");

        bool ftsExisted = db.tables().contains("note_fts");
        if(!query.exec(CREATE_NOTE_FTS_TABLE))   {
            qCritical() << "unable to create FTS index — full-text search disabled";
            qDebug() << query.lastError();
        }   else    {
            query.exec(CREATE_FTS_TRIGGER_AI);
            query.exec(CREATE_FTS_TRIGGER_AD);
            query.exec(CREATE_FTS_TRIGGER_AU);
            if(!ftsExisted)  {
                query.exec(BACKFILL_NOTE_FTS);
            }
        }
        return query;
    }
}
