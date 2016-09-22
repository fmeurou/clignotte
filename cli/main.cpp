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
#include <QIntValidator>

#include <iostream>

#include <clignotte.h>

QTextStream out(stdout);
QIntValidator v(0,1000000);

void displayNotebooks(Clignotte clignotte)    {
    QList<Notebook> notebooks = clignotte.notebooks(0);
    out  << "  title (* last_used) \n";
    out  << QString("").fill('-',80) << "\n";
    out.flush();
    foreach (Notebook notebook, notebooks) {
        QString lastUsed = "";
        if(notebook.getLastUsed()) lastUsed="(*)";
        out
                << notebook.getTitle()
                << lastUsed
                << "\n";
    }
    out.flush();
}

void listDisplayNotes(Clignotte clignotte)  {
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
                << qSetFieldWidth(20) << note.getNotebook().getTitle()
                << qSetFieldWidth(1) << "|"
                << qSetFieldWidth(20) << note.getDueDate().toString(Qt::ISODate).left(20)
                << qSetFieldWidth(1) << "|"
                << qSetFieldWidth(4) << note.isDone()
                << qSetFieldWidth(1) << "|"
                << qSetFieldWidth(81) << note.getText()
                << qSetFieldWidth(1) << "|";
        out << NORMAL_TEXT;
        out << "\n";
    }
    out.flush();
    exit(0);
}

void prettyDisplayNotes(Clignotte clignotte)   {
    QList<Notebook> notebooks = clignotte.notebooks(Clignotte::Active);
    foreach(Notebook notebook, notebooks)   {
        out << notebook.getTitle() << "\n";
        out.flush();
        foreach(Note note, notebook.notes())    {
            out  << "\t";
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
            out << "- " << note.getText();
            if(note.isDue()) {
                out << note.getDueDate().toString(" (yyyy-MM-dd)");
            }
            out << NORMAL_TEXT;
            out << "\n";
        }
        out.flush();
    }
    exit(0);
}

void displayHelp()  {
    out << "usage: note <action> <options>\n"
        << "actions: \n\tno action: display a pretty list of notes"
        << "\n\tlist: display a complete note list"
        << "\n\tnotebooks: display a list of notebooks"
        << "\n\tnotebook <notebook name>: create or set active notebook with name"
        << "\n\tadd <text>: add a note to the active notebook"
        << "\n\tdue <note id> <date>: set due date for that note"
        << "\n\tclose <note id>: mark the note as closed"
        << "\n\tdelete <note id>: delete note"
        << "\n\nNB: using ! or * at the beginning of the note marks them as important and bolded";
    out.flush();
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
        prettyDisplayNotes(clignotte);
        exit(0);
    }
    if(args.at(0) == "list")  {
        listDisplayNotes(clignotte);
        exit(0);
    }
    // source is args.at(0), destination is args.at(1)
    else if(args.at(0) == "notebook")   {
        if(args.length() < 2) {
            qWarning() << "invalid arguments count";
            qInfo() << "./note notebook <name ofnotebook> : creates or sets active notebook";
            exit(-1);
        }
        else {
            clignotte.getNotebook(args.mid(1).join(" "));
        }
        exit(0);
    }

    else if(args.at(0) == "notebooks")  {
        displayNotebooks(clignotte);
        exit(0);
    }


    else if(args.at(0) == "add")   {
        if(args.length() < 2) {
            qWarning() << "invalid arguments count";
            exit(-1);
        }
        qDebug() << args.mid(1).join(" ") << currentNotebook["id"];
        clignotte.createNote(args.mid(1).join(" "));
        exit(0);
    }

    else if(args.at(0) == "close")   {
        if(args.length() < 2) {
            qWarning() << "invalid arguments count";
            exit(-1);
        }
        QString noteId = args.at(1);
        int pos = 0;
        if(v.validate(noteId, pos) == QValidator::Invalid)  {
            qCritical() << "note id is not valid";
            exit(-1);
        }
        Note note = clignotte.getNote(QVariant(args.at(1)).toInt());
        note.setDone();
        exit(0);
    }

    else if(args.at(0) == "due")   {
        if(args.length() < 2) {
            qWarning() << "invalid arguments count";
            exit(-1);
        }
        QString noteId = args.at(1);
        int pos = 0;
        if(v.validate(noteId, pos) == QValidator::Invalid)  {
            qCritical() << "note id is not valid";
            exit(-1);
        }
        QDate date = QDate::fromString(args.at(2), "yyyy-MM-dd");
        if(!date.isValid())  {
            qWarning() << "invalid date format, use yyyy-MM-dd";
            exit(-1);
        }
        Note note = clignotte.getNote(QVariant(args.at(1)).toInt());
        note.updateDueDate(date);
        exit(0);
    }

    else if(args.at(0) == "encrypt")   {
        if(args.length() < 2) {
            qWarning() << "invalid arguments count";
            exit(-1);
        }

        QString noteId = args.at(1);
        int pos = 0;
        if(v.validate(noteId, pos) == QValidator::Invalid)  {
            qCritical() << "note id is not valid";
            exit(-1);
        }
        Note note = clignotte.getNote(QVariant(args.at(1)).toInt());
        if(args.length() > 2) {
            note.encrypt(args.at(2));
        }   else    {
            note.encrypt();
        }
        exit(0);
    }

    else if(args.at(0) == "read")   {
        if(args.length() < 2) {
            qWarning() << "invalid arguments count";
            exit(-1);
        }
        QString noteId = args.at(1);
        int pos = 0;
        if(v.validate(noteId, pos) == QValidator::Invalid)  {
            qCritical() << "note id is not valid";
            exit(-1);
        }
        Note note = clignotte.getNote(QVariant(args.at(1)).toInt());
        out << note.decrypt();
        exit(0);
    }

    else if(args.at(0) == "delete")   {
        if(args.length() < 2) {
            qWarning() << "invalid arguments count";
            exit(-1);
        }
        QString noteId = args.at(1);
        int pos = 0;
        if(v.validate(noteId, pos) == QValidator::Invalid)  {
            qCritical() << "note id is not valid";
            exit(-1);
        }
        Note note = clignotte.getNote(QVariant(args.at(1)).toInt());
        note.remove();
        exit(0);
    }
    else if(args.at(0) == "help")    {
        displayHelp();

    } else if(args.at(0) == "clear")    {
        clignotte.clearDb();

    } else    {
        qCritical() << "invalid command";
        displayHelp();

    }
}
