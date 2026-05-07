#include "sync.h"
#include "common.h"
#include "sql.h"

#include <QDate>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QList>
#include <QPair>
#include <QSet>
#include <QSqlError>
#include <QSqlRecord>
#include <QStringList>
#include <QUuid>
#include <QVariant>

namespace {
struct ExportedNote   {
    QString uuid;
    QString notebook;
    QDateTime createdAt;
    QDate dueDate;
    QDateTime doneAt;
    QStringList tags;
    QString text;
};

QString serializeExportedNote(const ExportedNote &n)   {
    QString s;
    s += "uuid: "       + n.uuid + "\n";
    s += "notebook: "   + n.notebook + "\n";
    s += "created_at: " + (n.createdAt.isValid() ? n.createdAt.toString(Qt::ISODateWithMs) : QString()) + "\n";
    s += "due_date: "   + (n.dueDate.isValid()   ? n.dueDate.toString(Qt::ISODate)         : QString()) + "\n";
    s += "done_at: "    + (n.doneAt.isValid()    ? n.doneAt.toString(Qt::ISODateWithMs)    : QString()) + "\n";
    s += "tags: "       + n.tags.join(", ") + "\n";
    s += "\n";
    s += n.text;
    if(!s.endsWith('\n')) s += "\n";
    return s;
}

bool parseExportedNote(const QString &content, ExportedNote &n)   {
    int sep = content.indexOf("\n\n");
    if(sep < 0) return false;
    QString meta = content.left(sep);
    QString body = content.mid(sep + 2);
    if(body.endsWith('\n')) body.chop(1);
    n.text = body;

    for(const QString &line : meta.split('\n'))   {
        int colon = line.indexOf(':');
        if(colon < 0) continue;
        QString key = line.left(colon).trimmed();
        QString val = line.mid(colon + 1).trimmed();
        if(key == "uuid")            n.uuid = val;
        else if(key == "notebook")   n.notebook = val;
        else if(key == "created_at") n.createdAt = QDateTime::fromString(val, Qt::ISODateWithMs);
        else if(key == "due_date")   n.dueDate   = val.isEmpty() ? QDate()     : QDate::fromString(val, Qt::ISODate);
        else if(key == "done_at")    n.doneAt    = val.isEmpty() ? QDateTime() : QDateTime::fromString(val, Qt::ISODateWithMs);
        else if(key == "tags")   {
            n.tags.clear();
            for(const QString &t : val.split(',', Qt::SkipEmptyParts))   {
                QString tt = t.trimmed();
                if(!tt.isEmpty()) n.tags << tt;
            }
        }
    }
    return !n.uuid.isEmpty();
}
} // namespace

void exportNotes(QSqlQuery &query, QSqlDatabase db, const QString &dirPath)   {
    QDir d(dirPath);
    if(!d.exists() && !d.mkpath("."))   {
        out << "unable to create directory: " << dirPath << "\n";
        out.flush();
        return;
    }

    if(!query.exec("SELECT note.id, note.uuid, note.created_at, note.due_date, note.done_at, note.text, notebook.title FROM note INNER JOIN notebook ON note.notebook=notebook.id"))   {
        qCritical() << "error querying notes";
        qDebug() << query.lastError() << db.lastError();
        return;
    }

    QSet<QString> writtenUuids;
    int written = 0;
    db.transaction();
    while(query.next())   {
        QSqlRecord r = query.record();
        int noteId = r.value(0).toInt();
        ExportedNote n;
        n.uuid = r.value(1).toString();
        if(n.uuid.isEmpty())   {
            n.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
            QSqlQuery uq;
            uq.prepare("UPDATE note SET uuid=:u WHERE id=:id");
            uq.bindValue(":u", n.uuid);
            uq.bindValue(":id", noteId);
            uq.exec();
        }
        n.createdAt = r.value(2).toDateTime();
        n.dueDate   = r.value(3).toDate();
        n.doneAt    = r.value(4).toDateTime();
        n.text      = r.value(5).toString();
        n.notebook  = r.value(6).toString();
        QSqlQuery tq;
        tq.prepare(QString(LIST_NOTE_TAGS));
        tq.bindValue(":note", noteId);
        if(tq.exec())   {
            while(tq.next()) n.tags << tq.value(0).toString();
        }
        QFile f(d.filePath(n.uuid + ".md"));
        if(!f.open(QIODevice::WriteOnly | QIODevice::Truncate))   {
            qWarning() << "unable to write" << f.fileName() << "-" << f.errorString();
            continue;
        }
        f.write(serializeExportedNote(n).toUtf8());
        f.close();
        writtenUuids.insert(n.uuid);
        written++;
    }
    db.commit();

    int pruned = 0;
    for(const QString &name : d.entryList({"*.md"}, QDir::Files))   {
        QString uuid = name.left(name.length() - 3);
        if(!writtenUuids.contains(uuid))   {
            d.remove(name);
            pruned++;
        }
    }

    out << "exported " << written << " note(s) to " << dirPath;
    if(pruned) out << "; pruned " << pruned << " orphan file(s)";
    out << "\n";
    out.flush();
}

void syncNotes(QSqlDatabase db, const QString &dirPath, bool deleteLocal)   {
    QDir d(dirPath);
    if(!d.exists())   {
        out << "directory not found: " << dirPath << "\n";
        out.flush();
        return;
    }
    QStringList files = d.entryList({"*.md"}, QDir::Files);

    QSet<QString> seenUuids;
    int created = 0, updated = 0, unchanged = 0, skipped = 0;
    db.transaction();
    for(const QString &name : files)   {
        QFile f(d.filePath(name));
        if(!f.open(QIODevice::ReadOnly))   {
            qWarning() << "unable to read" << f.fileName();
            skipped++;
            continue;
        }
        QString content = QString::fromUtf8(f.readAll());
        f.close();
        ExportedNote n;
        if(!parseExportedNote(content, n))   {
            qWarning() << "skipping" << name << "- failed to parse";
            skipped++;
            continue;
        }
        seenUuids.insert(n.uuid);

        QSqlQuery nq;
        nq.prepare(QString(SELECT_NOTEBOOK_BY_TITLE));
        nq.bindValue(":title", n.notebook);
        nq.exec();
        int notebookId;
        if(nq.first())   {
            notebookId = nq.value(0).toInt();
        }   else   {
            QSqlQuery ins;
            ins.prepare("INSERT INTO notebook(title, last_used) VALUES(:title, 0)");
            ins.bindValue(":title", n.notebook);
            ins.exec();
            notebookId = ins.lastInsertId().toInt();
        }

        QSqlQuery noteQ;
        noteQ.prepare("SELECT id, created_at, due_date, done_at, text, notebook FROM note WHERE uuid=:u");
        noteQ.bindValue(":u", n.uuid);
        noteQ.exec();
        int noteId;
        if(noteQ.first())   {
            noteId = noteQ.value(0).toInt();
            bool same = noteQ.value(1).toDateTime() == n.createdAt
                     && noteQ.value(2).toDate()     == n.dueDate
                     && noteQ.value(3).toDateTime() == n.doneAt
                     && noteQ.value(4).toString()   == n.text
                     && noteQ.value(5).toInt()      == notebookId;
            if(!same)   {
                QSqlQuery up;
                up.prepare("UPDATE note SET notebook=:nb, created_at=:ca, due_date=:dd, done_at=:da, text=:t WHERE id=:id");
                up.bindValue(":nb", notebookId);
                up.bindValue(":ca", n.createdAt);
                up.bindValue(":dd", n.dueDate.isValid() ? QVariant(n.dueDate)  : QVariant());
                up.bindValue(":da", n.doneAt.isValid()  ? QVariant(n.doneAt)   : QVariant());
                up.bindValue(":t",  n.text);
                up.bindValue(":id", noteId);
                up.exec();
                updated++;
            }   else   {
                unchanged++;
            }
        }   else   {
            QSqlQuery ins;
            ins.prepare("INSERT INTO note(notebook, uuid, created_at, due_date, done_at, text) VALUES(:nb, :u, :ca, :dd, :da, :t)");
            ins.bindValue(":nb", notebookId);
            ins.bindValue(":u",  n.uuid);
            ins.bindValue(":ca", n.createdAt);
            ins.bindValue(":dd", n.dueDate.isValid() ? QVariant(n.dueDate)  : QVariant());
            ins.bindValue(":da", n.doneAt.isValid()  ? QVariant(n.doneAt)   : QVariant());
            ins.bindValue(":t",  n.text);
            ins.exec();
            noteId = ins.lastInsertId().toInt();
            created++;
        }

        QSqlQuery dt;
        dt.prepare("DELETE FROM note_tag WHERE note=:n");
        dt.bindValue(":n", noteId);
        dt.exec();
        for(const QString &tag : n.tags)   {
            QSqlQuery it;
            it.prepare(QString(INSERT_NOTE_TAG));
            it.bindValue(":note", noteId);
            it.bindValue(":tag",  tag);
            it.exec();
        }
    }

    int deleted = 0;
    if(deleteLocal)   {
        QSqlQuery lq;
        lq.exec("SELECT id, uuid FROM note WHERE uuid IS NOT NULL AND uuid != ''");
        QList<QPair<int, QString>> locals;
        while(lq.next()) locals.append({lq.value(0).toInt(), lq.value(1).toString()});
        for(const auto &p : locals)   {
            if(!seenUuids.contains(p.second))   {
                QSqlQuery dq;
                dq.prepare(QString(DELETE_NOTE));
                dq.bindValue(":id", p.first);
                dq.exec();
                deleted++;
            }
        }
    }
    db.commit();

    out << "sync: " << created << " created, " << updated << " updated, " << unchanged << " unchanged";
    if(deleteLocal) out << ", " << deleted << " deleted";
    if(skipped)     out << ", " << skipped << " skipped";
    out << "\n";
    out.flush();
}
