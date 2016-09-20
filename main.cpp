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

QSqlQuery initDb(QSqlDatabase db)   {
    if(!db.open()) {
        qDebug() << "unable to open db";
        std::cout  << db.lastError().text().toStdString();
        exit(-2);
    }   else    {
        QSqlQuery query = QSqlQuery(db);
        if(!db.tables().contains("note"))  {
            qDebug() << "table note is not present, creating";
            if(!(query.exec(QString("CREATE TABLE notebook(id INTEGER PRIMARY KEY, title TEXT, last_used BOOL);"))
                    &&
                 query.exec(QString("CREATE TABLE note(id INTEGER PRIMARY KEY, created_at DATETIME, text TEXT, notebook INTEGER, FOREIGN KEY(notebook) REFERENCES notebook(id));"))
                 ))
            {
                qDebug() << query.lastQuery() << query.lastError() << db.lastError();
                std::cout  << "error creating database";
                exit(-1);
            }   else    {
                std::cout  << "inserting default notebook";
                query.exec(QString("INSERT INTO notebook(id, title, last_used) values(0, 'default', 1);"));

            }
        }
        return query;
    }

}

void list(QSqlQuery query, QSqlDatabase db) {
    QString listQuery;
    listQuery = "select notebook.title, note.id, note.created_at, note.text FROM note INNER JOIN notebook ON note.notebook=notebook.id";
    if(query.exec(listQuery))    {
        if(query.first())   {
            QSqlRecord record;
            std::cout  << "  notebook   |   id   |    creation date    |  note \n";
            std::cout  << QString("").fill('-',80).toStdString() << "\n";
            do  {
                record = query.record();
                std::cout
                        << record.value(0).toString().toStdString()
                        << " | "
                        << QString().fill(' ', 6-record.value(0).toString().length()).toStdString()
                        << record.value(1).toString().toStdString()
                        << " | "
                        << record.value(2).toDateTime().toString(Qt::ISODate).toStdString()
                        << " | "
                        << record.value(3).toString().toStdString()
                        << "\n";

            }   while(query.next());
        }   else    {
            std::cout  << QCoreApplication::translate("main", "No note available\n").toStdString();
        }
    } else    {
        std::cout  << "error querying database\n";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
    }
    exit(0);
}

void pretty(QSqlQuery query, QSqlDatabase db)   {
    QString listQuery;
    listQuery = "SELECT DISTINCT notebook.id, notebook.title FROM note INNER JOIN notebook ON note.notebook=notebook.id";
    if(query.exec(listQuery))    {
        if(query.first())   {
            QSqlRecord record;
            do  {
                record = query.record();
                std::cout << record.value(1).toString().toStdString() << "\n";
                QSqlQuery subquery;
                subquery.prepare("select note.text FROM note INNER JOIN notebook ON note.notebook=notebook.id WHERE notebook.id=:notebook");
                subquery.bindValue(":notebook", record.value(0));
                if(subquery.exec()) {
                    if(subquery.first())   {
                        QSqlRecord subrecord;
                        do  {
                            subrecord = subquery.record();
                            std::cout  << "\t- " << subrecord.value(0).toString().toStdString() << "\n";
                        }   while(subquery.next());
                    }   else    {
                        std::cout  << QCoreApplication::translate("main", "No note available\n").toStdString();
                    }
                } else    {
                    std::cout  << "error querying database\n";
                    qDebug() << subquery.lastQuery() << subquery.lastError() << db.lastError();
                }

            }   while(query.next());
        }   else    {
            std::cout  << QCoreApplication::translate("main", "No note available\n").toStdString();
        }
    } else    {
        std::cout  << "error querying database\n";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
    }
    exit(0);
}

void addNote(QSqlQuery query, QSqlDatabase db, QString text, QMap<QString, QString> currentNotebook)    {
    query.prepare(QString("insert into note(notebook, created_at, text) values(:notebook, :currentDateTime, :text)"));
    query.bindValue(":currentDateTime", QDateTime::currentDateTime());
    query.bindValue(":notebook", currentNotebook["id"]);
    query.bindValue(":text", text);
    if(query.exec())  {
        std::cout  << "note added\n";
    } else    {
        std::cout  << "error saving note to database\n";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
    }
}

void deleteNote(QSqlQuery query, QSqlDatabase db, QString note) {
    int noteId = QVariant(note).toInt();
    if(noteId)    {
        query.prepare(QString("delete from note where id=:id"));
        query.bindValue(":id", noteId);
        if(query.exec())  {
            std::cout  << "note deleted\n";
        } else    {
            qDebug() << query.lastQuery() << query.lastError() << db.lastError();
            std::cout  << "error deleting note from database\n";
        }
    }    else    {
        std::cout  << "invalid note identifier\n";
    }
}

QMap<QString, QString> getCurrentNotebook(QSqlQuery query, QSqlDatabase db)    {
    QMap<QString, QString> currentNotebook;
    if(query.exec("SELECT id, title FROM notebook WHERE last_used = 1"))    {
        if(query.first())   {
            QSqlRecord record;
            record = query.record();
            currentNotebook["id"] = record.value(0).toString();
            currentNotebook["name"] = record.value(1).toString();
        }
    } else    {
        std::cout  << "error querying database\n";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
    }
    return currentNotebook;
}

void setNotebookByTitle(QSqlQuery query, QSqlDatabase db, QString title)    {
    query.prepare("SELECT id, title FROM notebook WHERE title=:title");
    query.bindValue(":title", title);
    query.exec();
    if(query.first())   {
        QSqlRecord record;
        record = query.record();
        query.exec("UPDATE notebook SET last_used=0");
        query.exec(QString("UPDATE notebook SET last_used=1 WHERE id=%1").arg(record.value(0).toString()));
        std::cout  << "notebook selected\n";
    } else    {
        query.exec("UPDATE notebook SET last_used=0");
        query.prepare("insert into notebook values(NULL, :title, 1);");
        query.bindValue(":title", title);
        if(query.exec())  {
            std::cout  << "notebook added\n";
        } else    {
            std::cout  << "error saving notebook to database\n";
            qDebug() << query.lastQuery() << query.lastError() << db.lastError();
        }
    }
}

void listNotebooks(QSqlQuery query, QSqlDatabase db)    {
    QString listQuery;
    listQuery = "SELECT title, last_used FROM notebook ORDER BY last_used, title";
    if(query.exec(listQuery))    {
        if(query.first())   {
            QSqlRecord record;
            std::cout  << "  title (* last_used) \n";
            std::cout  << QString("").fill('-',80).toStdString() << "\n";
            do  {
                record = query.record();
                QString lastUsed = "";
                if(record.value(1).toBool()) lastUsed="(*)";
                std::cout
                        << record.value(0).toString().toStdString()
                        << lastUsed.toStdString()
                        << "\n";
            }   while(query.next());
        }   else    {
            std::cout  << QCoreApplication::translate("main", "No note available\n").toStdString();
        }
    } else    {
        std::cout  << "error querying database\n";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
    }
}


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QString notebookTable = "notebook";
    QString noteTable = "note";
    QString currentNotebookId = 0;
    QString currentNotebookName = "";
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
            std::cout  << "no notebook to choose or create...";
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
            std::cout  << "no note to add...";
        }
        else {
            qDebug() << args.mid(1).join(" ") << currentNotebookId;
            addNote(query, db, args.mid(1).join(" "), currentNotebook);

        }
        exit(0);
    }

    if(args.at(0) == "delete")   {
        if(args.length() < 2) {
            std::cout  << "no note to delete...\n";
        }
        else {
            deleteNote(query, db, args.at(1));
        }
        exit(0);
    }

    return app.exec();
}
