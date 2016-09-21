#include <QCoreApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QStandardPaths>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QTextStream>
#include <QDebug>

#include <iostream>

#include <clignotte.h>

QTextStream out(stdout);

void displayNotebooks(QSqlQuery query, QSqlDatabase db)    {
    QList<Notebook> notebooks;
    out  << "  title (* last_used) \n";
    out  << QString("").fill('-',80) << "\n";
    out.flush();
    foreach (Notebook notebook, notebooks) {
        QString lastUsed = "";
        if(notebook.getlastUsed()) lastUsed="(*)";
        out
                << notebook.getTitle()
                << lastUsed
                << "\n";
    }
    out.flush();
}

void displayNotes(QTextStream out, Clignotte clignotte)  {
    QList<Note> notes = clignotte.notes();
    out.setFieldAlignment(QTextStream::AlignCenter);
    out     << UNDERLINED_TEXT<< "|"
            << qSetFieldWidth(4) << "id"
            << qSetFieldWidth(1) << "|"
            << qSetFieldWidth(20) << "notebook"
            << qSetFieldWidth(1) << "|"
            << qSetFieldWidth(20) << "due date"
            << qSetFieldWidth(1) << "|"
            << qSetFieldWidth(4) << "done"
            << qSetFieldWidth(1) << "|"
            << qSetFieldWidth(81) << "note"
            << qSetFieldWidth(1) << "|\n"
            << NORMAL_TEXT;
    out.setFieldAlignment(QTextStream::AlignLeft);
    foreach(Note note, notes)    {
        if(note.isDone())   {
            out << ITALIC_TEXT;
        }   else if(note.isDue())   {
            out << URGENT_TEXT;
        }   else if(note.isImportant())  {
            out << IMPORTANT_TEXT;
        }     else if(note.isBold()) {
            out << INVERTED_TEXT;
        }   else    {
            out << NORMAL_TEXT;
        }
        out     << qSetFieldWidth(1) << "|"
                << qSetFieldWidth(4) << note.getId()
                << qSetFieldWidth(1) << "|"
                << qSetFieldWidth(20) << note.getNotebook()
                << qSetFieldWidth(1) << "|"
                << qSetFieldWidth(20) << note.getDueDate().toString(Qt::ISODate).left(20)
                << qSetFieldWidth(1) << "|"
                << qSetFieldWidth(4) << note.getDoneBool()
                << qSetFieldWidth(1) << "|"
                << qSetFieldWidth(81) << note.getText()
                << qSetFieldWidth(1) << "|";
        out << NORMAL_TEXT;
        out << "\n";
    }
    out.flush();
    exit(0);
}

void pretty()   {
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

    Clignotte clignotte(storedNotes);
    clignotte.initDb();
    QCommandLineParser parser;
    parser.setApplicationDescription("Note helper");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("command", QCoreApplication::translate("main", "what to do (list, add, delete, due, close, notebook)"));
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
