#include "tags.h"
#include "common.h"
#include "sql.h"

#include <QDebug>
#include <QSqlError>
#include <QSqlRecord>

void tagNote(QSqlQuery &query, QSqlDatabase db, const QString &noteIdStr, const QStringList &tags)    {
    int noteId = noteIdStr.toInt();
    if(!noteId)    {
        qWarning() << "invalid note identifier";
        return;
    }
    query.prepare(QString(SELECT_NOTE_TEXT));
    query.bindValue(":id", noteId);
    if(!query.exec() || !query.first())    {
        out << "note not found\n";
        out.flush();
        return;
    }
    db.transaction();
    int added = 0;
    for(const QString &raw : tags)    {
        QString t = raw.trimmed();
        if(t.isEmpty()) continue;
        query.prepare(QString(INSERT_NOTE_TAG));
        query.bindValue(":note", noteId);
        query.bindValue(":tag", t);
        if(query.exec() && query.numRowsAffected() > 0) added++;
    }
    db.commit();
    out << added << " tag(s) added\n";
    out.flush();
}

void untagNote(QSqlQuery &query, QSqlDatabase db, const QString &noteIdStr, const QStringList &tags)    {
    int noteId = noteIdStr.toInt();
    if(!noteId)    {
        qWarning() << "invalid note identifier";
        return;
    }
    db.transaction();
    int removed = 0;
    for(const QString &raw : tags)    {
        QString t = raw.trimmed();
        if(t.isEmpty()) continue;
        query.prepare(QString(DELETE_NOTE_TAG));
        query.bindValue(":note", noteId);
        query.bindValue(":tag", t);
        if(query.exec()) removed += query.numRowsAffected();
    }
    db.commit();
    out << removed << " tag(s) removed\n";
    out.flush();
}

void listAllTags(QSqlQuery &query, QSqlDatabase db)    {
    if(!query.exec(QString(LIST_ALL_TAGS)))    {
        qCritical() << "error querying tags";
        qDebug() << query.lastError() << db.lastError();
        return;
    }
    if(!query.first())    {
        out << "no tags yet\n";
        out.flush();
        return;
    }
    out << UNDERLINED_TEXT << QString("%1  %2\n").arg("count", 5).arg("tag") << NORMAL_TEXT;
    do  {
        QSqlRecord r = query.record();
        out << QString("%1  %2\n").arg(r.value(1).toInt(), 5).arg(r.value(0).toString());
    }   while(query.next());
    out.flush();
}

void listNoteTags(QSqlQuery &query, QSqlDatabase db, const QString &noteIdStr)    {
    int noteId = noteIdStr.toInt();
    if(!noteId)    {
        qWarning() << "invalid note identifier";
        return;
    }
    query.prepare(QString(LIST_NOTE_TAGS));
    query.bindValue(":note", noteId);
    if(!query.exec())    {
        qCritical() << "error querying tags";
        qDebug() << query.lastError() << db.lastError();
        return;
    }
    if(!query.first())    {
        out << "no tags on note " << noteId << "\n";
        out.flush();
        return;
    }
    QStringList result;
    do  {
        result << query.record().value(0).toString();
    }   while(query.next());
    out << result.join(" ") << "\n";
    out.flush();
}
