

#include <QCoreApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QStandardPaths>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>

#include <iostream>

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
