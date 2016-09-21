

#include <QCoreApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QStandardPaths>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QDebug>
#include <QTextStream>
#include <QDateTime>
#include <iostream>

// DEFINE queries
#define CREATE_NOTEBOOK_TABLE "CREATE TABLE notebook(id INTEGER PRIMARY KEY, title TEXT, last_used BOOL);"
#define CREATE_NOTE_TABLE "CREATE TABLE note(id INTEGER PRIMARY KEY, created_at DATETIME, due_date DATE, done_at DATETIME, text TEXT, notebook INTEGER, FOREIGN KEY(notebook) REFERENCES notebook(id));"
#define INIT_NOTEBOOK_TABLE "INSERT INTO notebook(id, title, last_used) values(0, 'default', 1);"
#define LIST_NOTES "SELECT notebook.title, note.id, note.due_date, note.done_at, note.text FROM note INNER JOIN notebook ON note.notebook=notebook.id ORDER BY done_at, due_date ASC, text"
#define LIST_ACTIVE_NOTEBOOKS "SELECT DISTINCT notebook.id, notebook.title FROM note INNER JOIN notebook ON note.notebook=notebook.id ORDER BY notebook.title"
#define LIST_NOTEBOOK_NOTES "select note.text, note.due_date, note.done_at FROM note INNER JOIN notebook ON note.notebook=notebook.id WHERE notebook.id=:notebook ORDER BY done_at, due_date ASC, text"
#define CURRENT_NOTEBOOK "SELECT id, title FROM notebook WHERE last_used = 1"
#define INSERT_NOTE "insert into note(notebook, created_at, text) values(:notebook, :currentDateTime, :text)"
#define UPDATE_DUE_DATE "UPDATE note SET due_date=:dueDate WHERE id=:id"
#define UPDATE_DONE "UPDATE note SET done_at=:currentDateTime WHERE id=:id"
#define DELETE_NOTE "delete from note where id=:id"
#define SELECT_NOTEBOOK_BY_TITLE "SELECT id, title FROM notebook WHERE title=:title"
#define RESET_LAST_USED "UPDATE notebook SET last_used=0"
#define UPDATE_LAST_USED "UPDATE notebook SET last_used=1 WHERE id=%1"
#define INSERT_NOTEBOOK "insert into notebook values(NULL, :title, 1);"
#define LIST_NOTEBOOKS "SELECT title, last_used FROM notebook ORDER BY last_used, title"

#define IMPORTANT_TEXT "\e[1;31m"
#define URGENT_TEXT "\e[7;31m"
#define BOLD_TEXT "\e[1m"
#define ITALIC_TEXT "\e[3m"
#define END_BOLD_TEXT "\e[21m"
#define INVERTED_TEXT "\e[7m"
#define END_INVERTED_TEXT "\e[27m"
#define NORMAL_TEXT "\e[0m"
#define UNDERLINED_TEXT "\e[4m"

QTextStream out(stdout);


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
        return query;
    }

}

void list(QSqlQuery query, QSqlDatabase db) {
    QString listQuery;
    out.setFieldAlignment(QTextStream::AlignCenter);
    listQuery = QString(LIST_NOTES);
    if(query.exec(listQuery))    {
        if(query.first())   {
            QSqlRecord record;
            out     << UNDERLINED_TEXT<< "|"
                    << qSetFieldWidth(20) << "notebook"
                    << qSetFieldWidth(1) << "|"
                    << qSetFieldWidth(4) << "id"
                    << qSetFieldWidth(1) << "|"
                    << qSetFieldWidth(20) << "due date"
                    << qSetFieldWidth(1) << "|"
                    << qSetFieldWidth(4) << "done"
                    << qSetFieldWidth(1) << "|"
                    << qSetFieldWidth(81) << "note"
                    << qSetFieldWidth(1) << "|\n"
                    << NORMAL_TEXT;
            out.setFieldAlignment(QTextStream::AlignLeft);
            do  {
                record = query.record();
                QString text = record.value(4).toString();
                bool isImportant = text.startsWith("!");
                bool isBold = text.startsWith("*");
                bool isDue = record.value(2).toDate().isValid() && record.value(2).toDate().operator <(QDate::currentDate());
                bool isDone = record.value(3).toBool();
                if(isDone)   {
                    out << ITALIC_TEXT;
                }   else if(isDue)   {
                    out << URGENT_TEXT;
                }   else if(isImportant)  {
                    out << IMPORTANT_TEXT;
                }     else if(isBold) {
                    out << INVERTED_TEXT;
                }   else    {
                    out << NORMAL_TEXT;
                }
                out     << qSetFieldWidth(1) << "|"
                        << qSetFieldWidth(20) << record.value(0).toString().left(20)
                        << qSetFieldWidth(1) << "|"
                        << qSetFieldWidth(4) << record.value(1).toString()
                        << qSetFieldWidth(1) << "|"
                        << qSetFieldWidth(20) << record.value(2).toDate().toString(Qt::ISODate).left(20)
                        << qSetFieldWidth(1) << "|"
                        << qSetFieldWidth(4) << record.value(3).toBool()
                        << qSetFieldWidth(1) << "|"
                        << qSetFieldWidth(81) << record.value(4).toString().left(31)
                        << qSetFieldWidth(1) << "|";
                out << NORMAL_TEXT;
                out << "\n";
            }   while(query.next());
            out.flush();
        }   else    {
            qInfo()  << QCoreApplication::translate("main", "No note available\n");
        }
    } else    {
        qCritical()  << "error querying database\n";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
    }
    exit(0);
}

void pretty(QSqlQuery query, QSqlDatabase db)   {
    QString listQuery;
    listQuery = QString(LIST_ACTIVE_NOTEBOOKS);
    if(query.exec(listQuery))    {
        if(query.first())   {
            QSqlRecord record;
            do  {
                record = query.record();
                out << record.value(1).toString() << "\n";
                out.flush();
                QSqlQuery subquery;
                subquery.prepare(QString(LIST_NOTEBOOK_NOTES));
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
                            bool isDue = dueDate.isValid() && (dueDate < QDate::currentDate());
                            bool isDone = subrecord.value(2).toBool();
                            out  << "\t";
                            if(isDone)   {
                                out << ITALIC_TEXT;
                            }   else if(isDue)   {
                                out << URGENT_TEXT;
                            }   else if(isImportant)  {
                                out << IMPORTANT_TEXT;
                            }     else if(isBold) {
                                out << INVERTED_TEXT;
                            }   else    {
                                out << NORMAL_TEXT;
                            }
                            out << "- " << subrecord.value(0).toString();
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

void addNote(QSqlQuery query, QSqlDatabase db, QString text, QMap<QString, QString> currentNotebook)    {
    query.prepare(QString(INSERT_NOTE));
    query.bindValue(":currentDateTime", QDateTime::currentDateTime());
    query.bindValue(":notebook", currentNotebook["id"]);
    query.bindValue(":text", text);
    if(query.exec())  {
        out  << "note added\n";
    } else    {
        qCritical() << "error saving note to database\n";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
    }
}

void setDueDate(QSqlQuery query, QSqlDatabase db, QString note, QString dueDateString)    {
    int noteId = QVariant(note).toInt();
    if(noteId)    {
        QDate dueDate = QDate::fromString(dueDateString, "yyyy-MM-dd");
        if(!dueDate.isValid())  {
            qWarning() << "invalid date format, use yyyy-MM-dd (2016-12-31)";
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

void endNote(QSqlQuery query, QSqlDatabase db, QString note)    {
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

void deleteNote(QSqlQuery query, QSqlDatabase db, QString note) {
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

QMap<QString, QString> getCurrentNotebook(QSqlQuery query, QSqlDatabase db)    {
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

void setNotebookByTitle(QSqlQuery query, QSqlDatabase db, QString title)    {
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

void listNotebooks(QSqlQuery query, QSqlDatabase db)    {
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


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName("clignotte");
    QCoreApplication::setApplicationVersion("1.0");
    QMap<QString, QString> currentNotebook;

    QString storedNotes = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if(!QDir(storedNotes).exists() && !QDir(storedNotes).mkpath(storedNotes)) {
        std::cout  << "unable to create directory, exiting...";
        exit(-1);
    }
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    QDir storage(storedNotes);
    db.setDatabaseName(storage.absoluteFilePath("notes.db"));
    QSqlQuery query = initDb(db);
    currentNotebook = getCurrentNotebook(query, db);
    QCommandLineParser parser;
    parser.setApplicationDescription("Note helper");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("command", QCoreApplication::translate("main", "what to do (list, add, delete)"));
    parser.addPositionalArgument("content", QCoreApplication::translate("main", "note id or text"));

    // Process the actual command line arguments given by the user
    parser.process(app);

    const QStringList args = parser.positionalArguments();
    if(!args.length())  {
        pretty(query, db);
        exit(0);
    }
    if(args.at(0) == "list")  {
        list(query, db);
        exit(0);
    }
    // source is args.at(0), destination is args.at(1)
    if(args.at(0) == "notebook")   {
        if(args.length() < 2) {
            out  << "no notebook to choose or create...";
        }
        else {
            setNotebookByTitle(query, db, args.mid(1).join(" "));
            currentNotebook = getCurrentNotebook(query, db);
        }
        exit(0);
    }

    if(args.at(0) == "notebooks")  {
        listNotebooks(query, db);
        exit(0);
    }


    if(args.at(0) == "add")   {
        if(args.length() < 2) {
            out  << "no note to add...";
        }
        else {
            qDebug() << args.mid(1).join(" ") << currentNotebook["id"];
            addNote(query, db, args.mid(1).join(" "), currentNotebook);

        }
        exit(0);
    }

    if(args.at(0) == "close")   {
        if(args.length() < 2) {
            out  << "no note to close...";
        }
        else {
            endNote(query, db, args.at(1));

        }
        exit(0);
    }

    if(args.at(0) == "due")   {
        if(args.length() < 2) {
            out  << "no note to update...";
        }
        else {
            setDueDate(query, db, args.at(1), args.at(2));

        }
        exit(0);
    }

    if(args.at(0) == "delete")   {
        if(args.length() < 2) {
            out  << "no note to delete...\n";
        }
        else {
            deleteNote(query, db, args.at(1));
        }
        exit(0);
    }

    return app.exec();
}
