#include "notes.h"
#include "common.h"
#include "render.h"
#include "sql.h"

#include <QCoreApplication>
#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlRecord>
#include <QTemporaryDir>
#include <QUuid>
#include <QVariant>
#include <cstdlib>

void list(QSqlQuery &query, QSqlDatabase db, bool jsonOut, bool includeClosed, const QStringList &tags, const QString &notebook) {
    bool ok;
    if(tags.isEmpty() && notebook.isEmpty() && !includeClosed)    {
        ok = query.exec(QString(LIST_NOTES));
    }   else    {
        QString sql = "SELECT notebook.title, note.id, note.due_date, note.done_at, note.text, "
                      "(SELECT COUNT(*) FROM attachment WHERE attachment.note=note.id) "
                      "FROM note INNER JOIN notebook ON note.notebook=notebook.id WHERE 1=1";
        if(!includeClosed)      sql += " AND note.done_at IS NULL";
        if(!notebook.isEmpty()) sql += " AND notebook.title=:notebook";
        sql += tagFilterClause(tags.size());
        sql += " ORDER BY note.id";
        query.prepare(sql);
        if(!notebook.isEmpty()) query.bindValue(":notebook", notebook);
        bindTags(query, tags);
        ok = query.exec();
    }
    if(ok)    {
        if(jsonOut) printNotesJson(query);
        else        printNoteTable(query, QCoreApplication::translate("main", "No note available\n"));
    } else    {
        qCritical()  << "error querying database\n";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
    }
    exit(0);
}

void agenda(QSqlQuery &query, QSqlDatabase db, int days, const QString &emptyMessage, bool jsonOut, bool includeClosed, const QStringList &tags, const QString &notebook)   {
    QDate until = QDate::currentDate().addDays(days);
    QString sql = "SELECT notebook.title, note.id, note.due_date, note.done_at, note.text, "
                  "(SELECT COUNT(*) FROM attachment WHERE attachment.note=note.id) "
                  "FROM note INNER JOIN notebook ON note.notebook=notebook.id "
                  "WHERE note.due_date IS NOT NULL AND note.due_date <= :until";
    if(!includeClosed)      sql += " AND note.done_at IS NULL";
    if(!notebook.isEmpty()) sql += " AND notebook.title=:notebook";
    sql += tagFilterClause(tags.size());
    sql += " ORDER BY note.due_date, note.id";
    query.prepare(sql);
    query.bindValue(":until", until);
    if(!notebook.isEmpty()) query.bindValue(":notebook", notebook);
    bindTags(query, tags);
    if(query.exec())    {
        if(jsonOut) printNotesJson(query);
        else        printNoteTable(query, emptyMessage);
    }   else    {
        qCritical() << "agenda query failed\n";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
    }
    exit(0);
}

void search(QSqlQuery &query, QSqlDatabase db, const QString &searchTerm, bool jsonOut, bool includeClosed, const QStringList &tags, const QString &notebook)   {
    bool ok;
    if(tags.isEmpty() && notebook.isEmpty() && !includeClosed)    {
        query.prepare(QString(SEARCH_NOTES));
        query.bindValue(":q", searchTerm);
        ok = query.exec();
    }   else    {
        QString sql = "SELECT notebook.title, note.id, note.due_date, note.done_at, note.text, "
                      "(SELECT COUNT(*) FROM attachment WHERE attachment.note=note.id) "
                      "FROM note_fts INNER JOIN note ON note.id=note_fts.rowid "
                      "INNER JOIN notebook ON note.notebook=notebook.id "
                      "WHERE note_fts MATCH :q";
        if(!includeClosed)      sql += " AND note.done_at IS NULL";
        if(!notebook.isEmpty()) sql += " AND notebook.title=:notebook";
        sql += tagFilterClause(tags.size());
        sql += " ORDER BY rank";
        query.prepare(sql);
        query.bindValue(":q", searchTerm);
        if(!notebook.isEmpty()) query.bindValue(":notebook", notebook);
        bindTags(query, tags);
        ok = query.exec();
    }
    if(ok)    {
        if(jsonOut) printNotesJson(query);
        else        printNoteTable(query, QCoreApplication::translate("main", "No matching note\n"));
    } else    {
        qCritical()  << "search failed\n";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
    }
    exit(0);
}

void pretty(QSqlQuery &query, QSqlDatabase db, bool includeClosed)   {
    QString listQuery = includeClosed ? QString(LIST_ALL_NOTEBOOKS_WITH_NOTES) : QString(LIST_ACTIVE_NOTEBOOKS);
    if(query.exec(listQuery))    {
        if(query.first())   {
            QSqlRecord record;
            do  {
                record = query.record();
                out << record.value(1).toString() << "\n";
                out.flush();
                QSqlQuery subquery;
                subquery.prepare(includeClosed ? QString(LIST_ALL_NOTEBOOK_NOTES) : QString(LIST_NOTEBOOK_NOTES));
                subquery.bindValue(":notebook", record.value(0));
                if(subquery.exec()) {
                    if(subquery.first())   {
                        QSqlRecord subrecord;
                        do  {
                            subrecord = subquery.record();
                            QString text = subrecord.value(0).toString();
                            QDate dueDate = subrecord.value(1).toDate();
                            bool isImportant = text.startsWith("!");
                            bool isBold = text.startsWith("*");
                            bool isBlink = text.startsWith("~");
                            bool isDue = dueDate.isValid() && (dueDate < QDate::currentDate());
                            bool isDone = subrecord.value(2).toBool();
                            bool hasAttachment = subrecord.value(3).toInt() > 0;
                            out  << "\t";
                            if(isDone)   {
                                out << ITALIC_TEXT;
                            }   else if(isDue)   {
                                out << URGENT_TEXT;
                            }   else if(isBlink)  {
                                out << BLINK_TEXT;
                            }   else if(isImportant)  {
                                out << IMPORTANT_TEXT;
                            }     else if(isBold) {
                                out << INVERTED_TEXT;
                            }   else    {
                                out << NORMAL_TEXT;
                            }
                            out << (hasAttachment ? "@ " : "- ") << subrecord.value(0).toString();
                            if(dueDate.isValid() && !isDone) {
                                out << dueDate.toString(" (yyyy-MM-dd)");
                            }
                            out << NORMAL_TEXT;
                            out << "\n";
                        }   while(subquery.next());
                        out.flush();
                    }   else    {
                        out  << QCoreApplication::translate("main", "No note available\n");
                        out.flush();
                    }
                } else    {
                    qCritical()  << "error querying database\n";
                    qDebug() << subquery.lastQuery() << subquery.lastError() << db.lastError();
                }

            }   while(query.next());
        }   else    {
            out  << QCoreApplication::translate("main", "No note available\n");
        }
    } else    {
        qCritical()  << "error querying database\n";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
    }
    exit(0);
}

void byTag(QSqlQuery &query, QSqlDatabase db, bool jsonOut, bool includeClosed, const QString &notebook)   {
    QStringList tags;
    QString outerSql = "SELECT DISTINCT note_tag.tag FROM note_tag";
    bool needsNoteJoin = !notebook.isEmpty() || !includeClosed;
    if(needsNoteJoin)   {
        outerSql += " INNER JOIN note ON note.id=note_tag.note";
        if(!notebook.isEmpty()) outerSql += " INNER JOIN notebook ON notebook.id=note.notebook";
        outerSql += " WHERE 1=1";
        if(!includeClosed)      outerSql += " AND note.done_at IS NULL";
        if(!notebook.isEmpty()) outerSql += " AND notebook.title=:notebook";
    }
    outerSql += " ORDER BY note_tag.tag COLLATE NOCASE";
    query.prepare(outerSql);
    if(!notebook.isEmpty()) query.bindValue(":notebook", notebook);
    if(!query.exec())   {
        qCritical() << "error querying tags";
        qDebug() << query.lastError() << db.lastError();
        return;
    }
    while(query.next()) tags << query.record().value(0).toString();

    if(jsonOut)   {
        QJsonArray result;
        for(const QString &tag : tags)   {
            QSqlQuery q;
            QString innerSql = "SELECT notebook.title, note.id, note.due_date, note.done_at, note.text, "
                               "(SELECT COUNT(*) FROM attachment WHERE attachment.note=note.id) "
                               "FROM note INNER JOIN notebook ON note.notebook=notebook.id "
                               "INNER JOIN note_tag ON note_tag.note=note.id "
                               "WHERE note_tag.tag=:tag COLLATE NOCASE";
            if(!includeClosed)      innerSql += " AND note.done_at IS NULL";
            if(!notebook.isEmpty()) innerSql += " AND notebook.title=:notebook";
            innerSql += " ORDER BY note.id";
            q.prepare(innerSql);
            q.bindValue(":tag", tag);
            if(!notebook.isEmpty()) q.bindValue(":notebook", notebook);
            q.exec();
            QJsonObject group;
            group["tag"] = tag;
            group["notes"] = collectNotesJson(q);
            result.append(group);
        }
        out << QJsonDocument(result).toJson(QJsonDocument::Indented);
        out.flush();
        return;
    }

    if(tags.isEmpty())   {
        out << "no tags yet\n";
        out.flush();
        return;
    }

    for(const QString &tag : tags)   {
        out << BOLD_TEXT << tag << NORMAL_TEXT << "\n";
        QSqlQuery q;
        QString innerSql = "SELECT note.id, note.text, note.due_date, note.done_at, "
                           "(SELECT COUNT(*) FROM attachment WHERE attachment.note=note.id) "
                           "FROM note INNER JOIN note_tag ON note_tag.note=note.id";
        if(!notebook.isEmpty()) innerSql += " INNER JOIN notebook ON notebook.id=note.notebook";
        innerSql += " WHERE note_tag.tag=:tag COLLATE NOCASE";
        if(!includeClosed)      innerSql += " AND note.done_at IS NULL";
        if(!notebook.isEmpty()) innerSql += " AND notebook.title=:notebook";
        innerSql += " ORDER BY note.id";
        q.prepare(innerSql);
        q.bindValue(":tag", tag);
        if(!notebook.isEmpty()) q.bindValue(":notebook", notebook);
        if(!q.exec())   {
            qCritical() << "error querying notes for tag" << tag;
            continue;
        }
        while(q.next())   {
            QSqlRecord r = q.record();
            int id = r.value(0).toInt();
            QString text = r.value(1).toString();
            QDate due = r.value(2).toDate();
            bool isDone = r.value(3).toBool();
            bool hasAttachment = r.value(4).toInt() > 0;
            bool isImportant = text.startsWith("!");
            bool isBold = text.startsWith("*");
            bool isBlink = text.startsWith("~");
            bool isDue = due.isValid() && due < QDate::currentDate();

            const char *style = NORMAL_TEXT;
            if(isDone)            style = ITALIC_TEXT;
            else if(isDue)        style = URGENT_TEXT;
            else if(isBlink)      style = BLINK_TEXT;
            else if(isImportant)  style = IMPORTANT_TEXT;
            else if(isBold)       style = INVERTED_TEXT;

            out << "\t" << style << QString("[%1] ").arg(id, 3)
                << (hasAttachment ? "@ " : "- ") << text;
            if(due.isValid() && !isDone) out << due.toString(" (yyyy-MM-dd)");
            out << NORMAL_TEXT << "\n";
        }
    }
    out.flush();
}

void addNote(QSqlQuery &query, QSqlDatabase db, QString text, QMap<QString, QString> currentNotebook)    {
    query.prepare(QString(INSERT_NOTE));
    query.bindValue(":currentDateTime", QDateTime::currentDateTime());
    query.bindValue(":notebook", currentNotebook["id"]);
    query.bindValue(":text", text);
    query.bindValue(":uuid", QUuid::createUuid().toString(QUuid::WithoutBraces));
    if(query.exec())  {
        out << query.lastInsertId().toInt() << "\n";
        out.flush();
    } else    {
        qCritical() << "error saving note to database\n";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
    }
}

void markNote(QSqlQuery &query, QSqlDatabase db, QString note, QChar prefix)    {
    int noteId = QVariant(note).toInt();
    if(!noteId)  {
        qWarning() << "invalid note identifier\n";
        return;
    }
    query.prepare(QString(SELECT_NOTE_TEXT));
    query.bindValue(":id", noteId);
    if(!query.exec() || !query.first())  {
        qWarning() << "note not found\n";
        return;
    }
    QString text = query.record().value(0).toString();
    if(text.startsWith('!') || text.startsWith('*') || text.startsWith('~'))  {
        text.remove(0, 1);
    }
    if(!prefix.isNull())  {
        text.prepend(prefix);
    }
    query.prepare(QString(UPDATE_NOTE_TEXT));
    query.bindValue(":text", text);
    query.bindValue(":id", noteId);
    if(query.exec())  {
        out << "note updated\n";
    } else    {
        qCritical() << "error updating note\n";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
    }
}

void editNote(QSqlQuery &query, QSqlDatabase db, const QString &noteIdStr)    {
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
    QString original = query.record().value(0).toString();

    QTemporaryDir tempDir;
    if(!tempDir.isValid())    {
        out << "unable to create temporary directory\n";
        out.flush();
        return;
    }
    QString filePath = QDir(tempDir.path()).filePath("note.txt");
    QFile f(filePath);
    if(!f.open(QIODevice::WriteOnly | QIODevice::Text))    {
        out << "unable to write temp file: " << f.errorString() << "\n";
        out.flush();
        return;
    }
    f.write(original.toUtf8());
    f.close();

    QString editor = qEnvironmentVariable("VISUAL");
    if(editor.isEmpty()) editor = qEnvironmentVariable("EDITOR");
    if(editor.isEmpty()) editor = "vi";

    QString cmd = QString("%1 \"%2\"").arg(editor, filePath);
    int rc = std::system(cmd.toLocal8Bit().constData());
    if(rc != 0)    {
        out << "editor exited with code " << rc << ", note unchanged\n";
        out.flush();
        return;
    }

    if(!f.open(QIODevice::ReadOnly | QIODevice::Text))    {
        out << "unable to read temp file: " << f.errorString() << "\n";
        out.flush();
        return;
    }
    QString updated = QString::fromUtf8(f.readAll());
    f.close();
    if(updated.endsWith('\n')) updated.chop(1);

    if(updated.trimmed().isEmpty())    {
        out << "empty content, note unchanged\n";
        out.flush();
        return;
    }
    if(updated == original)    {
        out << "no changes\n";
        out.flush();
        return;
    }

    query.prepare(QString(UPDATE_NOTE_TEXT));
    query.bindValue(":text", updated);
    query.bindValue(":id", noteId);
    if(query.exec())    {
        out << "note updated\n";
        out.flush();
    }   else    {
        qCritical() << "error updating note";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
    }
}

void moveNote(QSqlQuery &query, QSqlDatabase db, const QString &noteIdStr, const QString &notebookTitle)    {
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

    query.prepare(QString(SELECT_NOTEBOOK_BY_TITLE));
    query.bindValue(":title", notebookTitle);
    if(!query.exec())    {
        qCritical() << "error querying notebooks";
        qDebug() << query.lastError() << db.lastError();
        return;
    }
    QString notebookId;
    if(query.first())    {
        notebookId = query.record().value(0).toString();
    }   else    {
        QSqlQuery insert(db);
        insert.prepare("INSERT INTO notebook(title, last_used) VALUES(:title, 0)");
        insert.bindValue(":title", notebookTitle);
        if(!insert.exec())    {
            qCritical() << "error creating notebook";
            qDebug() << insert.lastError() << db.lastError();
            return;
        }
        notebookId = insert.lastInsertId().toString();
        out << "notebook '" << notebookTitle << "' created\n";
        out.flush();
    }

    query.prepare(QString(UPDATE_NOTE_NOTEBOOK));
    query.bindValue(":notebook", notebookId);
    query.bindValue(":id", noteId);
    if(query.exec())    {
        out << "note moved to '" << notebookTitle << "'\n";
        out.flush();
    }   else    {
        qCritical() << "error moving note";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
    }
}

void setDueDate(QSqlQuery &query, QSqlDatabase db, QString note, QString dueDateString)    {
    int noteId = QVariant(note).toInt();
    if(noteId)    {
        QDate dueDate = QDate::fromString(dueDateString, "yyyy-MM-dd");
        if(!dueDate.isValid())  {
            qWarning() << "invalid date format, use yyyy-MM-dd (2016-12-31)";
            return;
        }
        query.prepare(QString(UPDATE_DUE_DATE));
        query.bindValue(":dueDate", dueDate);
        query.bindValue(":id", noteId);
        if(query.exec())  {
            out  << "note updated\n";
        } else    {
            qCritical() << "error updating note\n";
            qDebug() << query.lastQuery() << query.lastError() << query.boundValues() << db.lastError();
        }
    }    else    {
        qWarning() << "invalid note identifier\n";
    }
}

void endNote(QSqlQuery &query, QSqlDatabase db, QString note)    {
    int noteId = QVariant(note).toInt();
    if(noteId)    {
        query.prepare(QString(UPDATE_DONE));
        query.bindValue(":currentDateTime", QDateTime::currentDateTime());
        query.bindValue(":id", noteId);
        if(query.exec())  {
            out  << "note closed\n";
        } else    {
            qCritical() << "error closing note\n";
            qDebug() << query.lastQuery() << query.lastError() << query.boundValues() << db.lastError();
        }
    }    else    {
        qWarning() << "invalid note identifier\n";
    }
}

void deleteNote(QSqlQuery &query, QSqlDatabase db, QString note) {
    int noteId = QVariant(note).toInt();
    if(noteId)    {
        query.prepare(QString(DELETE_NOTE));
        query.bindValue(":id", noteId);
        if(query.exec())  {
            qInfo()  << "note deleted\n";
        } else    {
            qCritical()  << "error deleting note from database\n";
            qDebug() << query.lastQuery() << query.lastError() << db.lastError();
        }
    }    else    {
        qWarning() << "invalid note identifier\n";
    }
}
