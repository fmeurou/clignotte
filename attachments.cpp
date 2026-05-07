#include "attachments.h"
#include "common.h"
#include "sql.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlRecord>
#include <QUuid>
#include <QVariant>

void attachFile(QSqlQuery &query, QSqlDatabase db, const QString &noteIdStr, const QString &path, bool copy, const QString &storageDir)    {
    int noteId = noteIdStr.toInt();
    if(!noteId)    {
        qWarning() << "invalid note identifier";
        return;
    }
    QString storedPath = path;
    QString originalPath;
    if(copy)    {
        bool isUrl = path.startsWith("http://") || path.startsWith("https://");
        if(isUrl)    {
            out << "cannot copy remote URLs, storing reference only\n";
            out.flush();
        }   else    {
            if(!QFileInfo::exists(path))    {
                out << "file not found: " << path << "\n";
                out.flush();
                return;
            }
            QString attachDir = QDir(storageDir).filePath("attachments");
            QDir().mkpath(attachDir);
            QString destName = QUuid::createUuid().toString(QUuid::WithoutBraces) + "_" + QFileInfo(path).fileName();
            QString destPath = QDir(attachDir).filePath(destName);
            if(!QFile::copy(path, destPath))    {
                out << "failed to copy file to " << destPath << "\n";
                out.flush();
                return;
            }
            originalPath = path;
            storedPath = destPath;
        }
    }
    query.prepare(INSERT_ATTACHMENT);
    query.bindValue(":note", noteId);
    query.bindValue(":path", storedPath);
    query.bindValue(":originalPath", originalPath.isEmpty() ? QVariant() : originalPath);
    query.bindValue(":createdAt", QDateTime::currentDateTime());
    if(query.exec())    {
        out << "attachment added\n";
    }   else    {
        qCritical() << "error adding attachment\n";
        qDebug() << query.lastError() << db.lastError();
    }
}

void listAttachments(QSqlQuery &query, QSqlDatabase db, const QString &noteIdStr)    {
    int noteId = noteIdStr.toInt();
    if(!noteId)    {
        qWarning() << "invalid note identifier";
        return;
    }
    query.prepare(LIST_NOTE_ATTACHMENTS);
    query.bindValue(":note", noteId);
    if(!query.exec())    {
        qCritical() << "error querying attachments\n";
        qDebug() << query.lastError() << db.lastError();
        return;
    }
    if(!query.first())    {
        out << "no attachments for note " << noteId << "\n";
        out.flush();
        return;
    }
    out << UNDERLINED_TEXT << QString("%1  %2\n").arg("id", 4).arg("path") << NORMAL_TEXT;
    do  {
        QSqlRecord r = query.record();
        QString id = r.value(0).toString();
        QString storedPath = r.value(1).toString();
        QString origPath = r.value(2).toString();
        QString display = storedPath;
        if(!origPath.isEmpty()) display += " (copied from: " + origPath + ")";
        out << QString("%1  %2\n").arg(id, 4).arg(display);
    }   while(query.next());
    out.flush();
}

void detachFile(QSqlQuery &query, QSqlDatabase db, const QString &attachIdStr)    {
    int attachId = attachIdStr.toInt();
    if(!attachId)    {
        qWarning() << "invalid attachment identifier";
        return;
    }
    query.prepare(DELETE_ATTACHMENT);
    query.bindValue(":id", attachId);
    if(query.exec())    {
        out << "attachment removed\n";
    }   else    {
        qCritical() << "error removing attachment\n";
        qDebug() << query.lastError() << db.lastError();
    }
}
