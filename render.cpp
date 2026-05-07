#include "render.h"
#include "common.h"
#include "sql.h"

#include <QDate>
#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonValue>
#include <QSqlRecord>

QString tagFilterClause(int n)    {
    QString s;
    for(int i = 0; i < n; ++i)    {
        s += QString(" AND EXISTS(SELECT 1 FROM note_tag WHERE note_tag.note=note.id AND note_tag.tag=:tag%1 COLLATE NOCASE)").arg(i);
    }
    return s;
}

void bindTags(QSqlQuery &query, const QStringList &tags)    {
    for(int i = 0; i < tags.size(); ++i)
        query.bindValue(QString(":tag%1").arg(i), tags[i]);
}

void printNoteTable(QSqlQuery &query, const QString &emptyMessage)   {
    if(!query.first())  {
        out << emptyMessage;
        out.flush();
        return;
    }
    const int notebookWidth = 12;
    const int fixedWidth = 4 + 2 + 1 + 2 + 1 + 2 + 10 + 2 + notebookWidth + 2;
    int textWidth = terminalCols() - fixedWidth;
    if(textWidth < 20) textWidth = 20;

    out << UNDERLINED_TEXT
        << QString("%1  %2  %3  %4  %5  %6\n")
            .arg("id", 4)
            .arg("s")
            .arg("a")
            .arg(QString("due").leftJustified(10))
            .arg(QString("notebook").leftJustified(notebookWidth))
            .arg(QString("note").leftJustified(textWidth))
        << NORMAL_TEXT;

    do  {
        QSqlRecord record = query.record();
        QString notebook = record.value(0).toString();
        QString id = record.value(1).toString();
        QDate dueDate = record.value(2).toDate();
        bool isDone = record.value(3).toBool();
        QString text = record.value(4).toString();
        bool hasAttachment = record.value(5).toInt() > 0;
        bool isImportant = text.startsWith("!");
        bool isBold = text.startsWith("*");
        bool isBlink = text.startsWith("~");
        bool isDue = dueDate.isValid() && dueDate < QDate::currentDate();

        const char *style = NORMAL_TEXT;
        QString icon = QString::fromUtf8("·");
        if(isDone)            { style = ITALIC_TEXT;    icon = QString::fromUtf8("✓"); }
        else if(isDue)        { style = URGENT_TEXT;    icon = "!"; }
        else if(isBlink)      { style = BLINK_TEXT;     icon = "~"; }
        else if(isImportant)  { style = IMPORTANT_TEXT; icon = "*"; }
        else if(isBold)       { style = INVERTED_TEXT;  icon = ">"; }

        QString attachIcon = hasAttachment ? "@" : " ";
        QString dueStr = dueDate.isValid() ? dueDate.toString(Qt::ISODate) : QString();

        out << style
            << QString("%1  %2  %3  %4  %5  %6")
                .arg(id, 4)
                .arg(icon)
                .arg(attachIcon)
                .arg(dueStr.leftJustified(10))
                .arg(notebook.left(notebookWidth).leftJustified(notebookWidth))
                .arg(text.left(textWidth))
            << NORMAL_TEXT << "\n";
    }   while(query.next());
    out.flush();
}

QJsonArray collectNotesJson(QSqlQuery &query)    {
    QJsonArray arr;
    if(query.first())    {
        do  {
            QSqlRecord r = query.record();
            QJsonObject obj;
            obj["notebook"] = r.value(0).toString();
            int noteId = r.value(1).toInt();
            obj["id"] = noteId;
            QDate due = r.value(2).toDate();
            obj["due_date"] = due.isValid() ? QJsonValue(due.toString(Qt::ISODate)) : QJsonValue();
            QDateTime done = r.value(3).toDateTime();
            obj["done_at"] = done.isValid() ? QJsonValue(done.toString(Qt::ISODate)) : QJsonValue();
            obj["text"] = r.value(4).toString();
            obj["attachments"] = r.value(5).toInt();
            QSqlQuery tq;
            tq.prepare(LIST_NOTE_TAGS);
            tq.bindValue(":note", noteId);
            QJsonArray tags;
            if(tq.exec())    {
                while(tq.next()) tags.append(tq.record().value(0).toString());
            }
            obj["tags"] = tags;
            arr.append(obj);
        }   while(query.next());
    }
    return arr;
}

void printNotesJson(QSqlQuery &query)    {
    out << QJsonDocument(collectNotesJson(query)).toJson(QJsonDocument::Indented);
    out.flush();
}
