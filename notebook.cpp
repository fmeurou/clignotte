#include "notebook.h"
#include "common.h"
#include "sql.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QSqlError>
#include <QSqlRecord>

QMap<QString, QString> getCurrentNotebook(QSqlQuery &query, QSqlDatabase db)    {
    QMap<QString, QString> currentNotebook;
    if(query.exec(CURRENT_NOTEBOOK))    {
        if(query.first())   {
            QSqlRecord record;
            record = query.record();
            currentNotebook["id"] = record.value(0).toString();
            currentNotebook["name"] = record.value(1).toString();
        }
    } else    {
        qCritical()  << "error querying database\n";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
    }
    return currentNotebook;
}

void setNotebookByTitle(QSqlQuery &query, QSqlDatabase db, QString title)    {
    query.prepare(SELECT_NOTEBOOK_BY_TITLE);
    query.bindValue(":title", title);
    query.exec();
    if(query.first())   {
        QSqlRecord record;
        record = query.record();
        query.exec(RESET_LAST_USED);
        query.exec(QString(UPDATE_LAST_USED).arg(record.value(0).toString()));
        out  << "notebook selected\n";
    } else    {
        query.exec(RESET_LAST_USED);
        query.prepare(INSERT_NOTEBOOK);
        query.bindValue(":title", title);
        if(query.exec())  {
            out  << "notebook added\n";
        } else    {
            qCritical()  << "error saving notebook to database\n";
            qDebug() << query.lastQuery() << query.lastError() << db.lastError();
        }
    }
}

void closeNotebook(QSqlQuery &query, QSqlDatabase db, const QString &title)    {
    query.prepare(SELECT_NOTEBOOK_BY_TITLE);
    query.bindValue(":title", title);
    if(!query.exec())    {
        qCritical() << "error querying notebook";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
        return;
    }
    if(!query.first())    {
        out << "notebook '" << title << "' not found\n";
        out.flush();
        return;
    }
    int notebookId = query.record().value(0).toInt();

    query.prepare(CLOSE_NOTEBOOK_NOTES);
    query.bindValue(":now", QDateTime::currentDateTime());
    query.bindValue(":notebook", notebookId);
    if(query.exec())    {
        int n = query.numRowsAffected();
        if(n <= 0)    out << "no open notes in '" << title << "'\n";
        else if(n==1) out << "closed 1 note in '" << title << "'\n";
        else          out << "closed " << n << " notes in '" << title << "'\n";
        out.flush();
    }   else    {
        qCritical() << "error closing notebook";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
    }
}

void listNotebooks(QSqlQuery &query, QSqlDatabase db)    {
    QString listQuery;
    listQuery = LIST_NOTEBOOKS;
    if(query.exec(listQuery))    {
        if(query.first())   {
            QSqlRecord record;
            out  << "  title (* last_used) \n";
            out  << QString("").fill('-',80) << "\n";
            out.flush();
            do  {
                record = query.record();
                QString lastUsed = "";
                if(record.value(1).toBool()) lastUsed="(*)";
                out
                        << record.value(0).toString()
                        << lastUsed
                        << "\n";
            }   while(query.next());
            out.flush();
        }   else    {
            qInfo()  << QCoreApplication::translate("main", "No note available\n");
        }
    } else    {
        qCritical()  << "error querying database\n";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
    }
}
