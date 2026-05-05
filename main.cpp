

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
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QTemporaryDir>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTime>
#include <iostream>
#include <sys/ioctl.h>
#include <unistd.h>

// DEFINE queries
#define CREATE_NOTEBOOK_TABLE "CREATE TABLE notebook(id INTEGER PRIMARY KEY, title TEXT, last_used BOOL);"
#define CREATE_NOTE_TABLE "CREATE TABLE note(id INTEGER PRIMARY KEY, created_at DATETIME, due_date DATE, done_at DATETIME, text TEXT, notebook INTEGER, FOREIGN KEY(notebook) REFERENCES notebook(id));"
#define INIT_NOTEBOOK_TABLE "INSERT INTO notebook(id, title, last_used) values(0, 'default', 1);"
#define LIST_NOTES "SELECT notebook.title, note.id, note.due_date, note.done_at, note.text FROM note INNER JOIN notebook ON note.notebook=notebook.id ORDER BY note.id"
#define LIST_ACTIVE_NOTEBOOKS "SELECT DISTINCT notebook.id, notebook.title FROM note INNER JOIN notebook ON note.notebook=notebook.id ORDER BY notebook.title"
#define LIST_NOTEBOOK_NOTES "select note.text, note.due_date, note.done_at FROM note INNER JOIN notebook ON note.notebook=notebook.id WHERE notebook.id=:notebook ORDER BY note.id"
#define CURRENT_NOTEBOOK "SELECT id, title FROM notebook WHERE last_used = 1"
#define INSERT_NOTE "insert into note(notebook, created_at, text) values(:notebook, :currentDateTime, :text)"
#define UPDATE_DUE_DATE "UPDATE note SET due_date=:dueDate WHERE id=:id"
#define UPDATE_DONE "UPDATE note SET done_at=:currentDateTime WHERE id=:id"
#define SELECT_NOTE_TEXT "SELECT text FROM note WHERE id=:id"
#define UPDATE_NOTE_TEXT "UPDATE note SET text=:text WHERE id=:id"
#define DELETE_NOTE "delete from note where id=:id"
#define SELECT_NOTEBOOK_BY_TITLE "SELECT id, title FROM notebook WHERE title=:title"
#define RESET_LAST_USED "UPDATE notebook SET last_used=0"
#define UPDATE_LAST_USED "UPDATE notebook SET last_used=1 WHERE id=%1"
#define INSERT_NOTEBOOK "insert into notebook values(NULL, :title, 1);"
#define LIST_NOTEBOOKS "SELECT title, last_used FROM notebook ORDER BY last_used, title"
#define CREATE_NOTE_FTS_TABLE "CREATE VIRTUAL TABLE IF NOT EXISTS note_fts USING fts5(text, content='note', content_rowid='id')"
#define CREATE_FTS_TRIGGER_AI "CREATE TRIGGER IF NOT EXISTS note_ai AFTER INSERT ON note BEGIN INSERT INTO note_fts(rowid, text) VALUES (new.id, new.text); END"
#define CREATE_FTS_TRIGGER_AD "CREATE TRIGGER IF NOT EXISTS note_ad AFTER DELETE ON note BEGIN INSERT INTO note_fts(note_fts, rowid, text) VALUES('delete', old.id, old.text); END"
#define CREATE_FTS_TRIGGER_AU "CREATE TRIGGER IF NOT EXISTS note_au AFTER UPDATE ON note BEGIN INSERT INTO note_fts(note_fts, rowid, text) VALUES('delete', old.id, old.text); INSERT INTO note_fts(rowid, text) VALUES (new.id, new.text); END"
#define BACKFILL_NOTE_FTS "INSERT INTO note_fts(rowid, text) SELECT id, text FROM note"
#define SEARCH_NOTES "SELECT notebook.title, note.id, note.due_date, note.done_at, note.text FROM note_fts INNER JOIN note ON note.id=note_fts.rowid INNER JOIN notebook ON note.notebook=notebook.id WHERE note_fts MATCH :q ORDER BY rank"

#define IMPORTANT_TEXT "\e[1;31m"
#define URGENT_TEXT "\e[7;31m"
#define BOLD_TEXT "\e[1m"
#define ITALIC_TEXT "\e[3m"
#define END_BOLD_TEXT "\e[21m"
#define INVERTED_TEXT "\e[7m"
#define END_INVERTED_TEXT "\e[27m"
#define NORMAL_TEXT "\e[0m"
#define UNDERLINED_TEXT "\e[4m"
#define BLINK_TEXT "\e[5;33m"

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
        bool ftsExisted = db.tables().contains("note_fts");
        if(!query.exec(CREATE_NOTE_FTS_TABLE))   {
            qCritical() << "unable to create FTS index — full-text search disabled";
            qDebug() << query.lastError();
        }   else    {
            query.exec(CREATE_FTS_TRIGGER_AI);
            query.exec(CREATE_FTS_TRIGGER_AD);
            query.exec(CREATE_FTS_TRIGGER_AU);
            if(!ftsExisted)  {
                query.exec(BACKFILL_NOTE_FTS);
            }
        }
        return query;
    }

}

int terminalCols()   {
    struct winsize w;
    if(ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0)   {
        return w.ws_col;
    }
    bool ok = false;
    int envCols = qgetenv("COLUMNS").toInt(&ok);
    if(ok && envCols > 0) return envCols;
    return 80;
}

void printNoteTable(QSqlQuery &query, const QString &emptyMessage)   {
    if(!query.first())  {
        out << emptyMessage;
        out.flush();
        return;
    }
    const int notebookWidth = 12;
    const int fixedWidth = 4 + 2 + 1 + 2 + 10 + 2 + notebookWidth + 2;
    int textWidth = terminalCols() - fixedWidth;
    if(textWidth < 20) textWidth = 20;

    out << UNDERLINED_TEXT
        << QString("%1  %2  %3  %4  %5\n")
            .arg("id", 4)
            .arg("s")
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

        QString dueStr = dueDate.isValid() ? dueDate.toString(Qt::ISODate) : QString();

        out << style
            << QString("%1  %2  %3  %4  %5")
                .arg(id, 4)
                .arg(icon)
                .arg(dueStr.leftJustified(10))
                .arg(notebook.left(notebookWidth).leftJustified(notebookWidth))
                .arg(text.left(textWidth))
            << NORMAL_TEXT << "\n";
    }   while(query.next());
    out.flush();
}

void list(QSqlQuery &query, QSqlDatabase db) {
    if(query.exec(QString(LIST_NOTES)))    {
        printNoteTable(query, QCoreApplication::translate("main", "No note available\n"));
    } else    {
        qCritical()  << "error querying database\n";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
    }
    exit(0);
}

void search(QSqlQuery &query, QSqlDatabase db, const QString &searchTerm)   {
    query.prepare(QString(SEARCH_NOTES));
    query.bindValue(":q", searchTerm);
    if(query.exec())    {
        printNoteTable(query, QCoreApplication::translate("main", "No matching note\n"));
    } else    {
        qCritical()  << "search failed\n";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
    }
    exit(0);
}

void pretty(QSqlQuery &query, QSqlDatabase db)   {
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
                            bool isBlink = text.startsWith("~");
                            bool isDue = dueDate.isValid() && (dueDate < QDate::currentDate());
                            bool isDone = subrecord.value(2).toBool();
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

void addNote(QSqlQuery &query, QSqlDatabase db, QString text, QMap<QString, QString> currentNotebook)    {
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

bool isAudioFile(const QString &path)   {
    static const QStringList exts = {
        "mp3", "wav", "ogg", "oga", "opus", "m4a", "mp4", "mpeg", "mpga",
        "flac", "aac", "webm", "wma", "amr", "mka", "3gp"
    };
    return exts.contains(QFileInfo(path).suffix().toLower());
}

void importAudio(QSqlQuery &query, QSqlDatabase db, const QString &path, const QMap<QString, QString> &currentNotebook)    {
    if(!QFileInfo::exists(path))   {
        out << "file not found: " << path << "\n";
        out.flush();
        return;
    }
    QTemporaryDir tempDir;
    if(!tempDir.isValid())   {
        out << "unable to create temporary directory\n";
        out.flush();
        return;
    }
    out << "transcribing " << path << " with whisper (this may take a while)...\n";
    out.flush();

    QProcess whisper;
    whisper.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    whisper.start("whisper", QStringList()
        << path
        << "--output_format" << "json"
        << "--output_dir" << tempDir.path());
    if(!whisper.waitForStarted(5000))   {
        out << "unable to start whisper. Install it with: pacman -S python-openai-whisper\n";
        out.flush();
        return;
    }
    whisper.waitForFinished(-1);
    if(whisper.exitStatus() != QProcess::NormalExit || whisper.exitCode() != 0)   {
        out << "whisper failed (exit code " << whisper.exitCode() << ")\n";
        out.flush();
        return;
    }

    QString jsonPath = QDir(tempDir.path()).filePath(QFileInfo(path).completeBaseName() + ".json");
    QFile jsonFile(jsonPath);
    if(!jsonFile.open(QIODevice::ReadOnly))   {
        out << "whisper output not found at " << jsonPath << "\n";
        out.flush();
        return;
    }
    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(jsonFile.readAll(), &perr);
    if(doc.isNull())   {
        out << "failed to parse whisper output: " << perr.errorString() << "\n";
        out.flush();
        return;
    }
    QJsonArray segments = doc.object().value("segments").toArray();
    if(segments.isEmpty())   {
        out << "whisper produced no segments\n";
        out.flush();
        return;
    }

    int count = 0;
    db.transaction();
    query.prepare(QString(INSERT_NOTE));
    for(const QJsonValue &v : segments)   {
        QString text = v.toObject().value("text").toString().trimmed();
        if(text.isEmpty()) continue;
        double startSec = v.toObject().value("start").toDouble();
        QString stamp = QTime(0, 0).addMSecs(int(startSec * 1000)).toString("HH:mm:ss");
        QString noteText = QString("[%1] %2").arg(stamp, text);
        query.bindValue(":currentDateTime", QDateTime::currentDateTime());
        query.bindValue(":notebook", currentNotebook["id"]);
        query.bindValue(":text", noteText);
        if(!query.exec())   {
            out << "error inserting segment: " << query.lastError().text() << "\n";
            out.flush();
            db.rollback();
            return;
        }
        count++;
    }
    db.commit();
    out << count << " note(s) imported from audio into notebook '" << currentNotebook["name"] << "'\n";
    out.flush();
}

void importNotes(QSqlQuery &query, QSqlDatabase db, const QString &path, const QMap<QString, QString> &currentNotebook)    {
    if(isAudioFile(path))   {
        importAudio(query, db, path, currentNotebook);
        return;
    }
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))   {
        out << "unable to open file: " << path << " - " << file.errorString() << "\n";
        out.flush();
        return;
    }
    QTextStream in(&file);
    int count = 0;
    db.transaction();
    query.prepare(QString(INSERT_NOTE));
    while(!in.atEnd())   {
        QString line = in.readLine();
        if(line.trimmed().isEmpty()) continue;
        query.bindValue(":currentDateTime", QDateTime::currentDateTime());
        query.bindValue(":notebook", currentNotebook["id"]);
        query.bindValue(":text", line);
        if(!query.exec())   {
            qCritical() << "error importing line:" << line;
            qDebug() << query.lastError() << db.lastError();
            db.rollback();
            return;
        }
        count++;
    }
    db.commit();
    out << count << " note(s) imported into notebook '" << currentNotebook["name"] << "'\n";
    out.flush();
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

QMap<QString, QString> getCurrentNotebook(QSqlQuery &query, QSqlDatabase db)    {
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

void setNotebookByTitle(QSqlQuery &query, QSqlDatabase db, QString title)    {
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

void listNotebooks(QSqlQuery &query, QSqlDatabase db)    {
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
    parser.setApplicationDescription(QCoreApplication::translate("main",
        "clignotte - command-line note keeper.\n"
        "\n"
        "Commands:\n"
        "  (none)               display notes grouped by notebook\n"
        "  list                 display notes in a table\n"
        "  add <text>           add a note to the current notebook\n"
        "  import <path>        add notes from a text file (one per line) or an audio file (one per whisper segment)\n"
        "  close <id>           mark note <id> as done\n"
        "  due <id> <date>      set due date (yyyy-MM-dd) for note <id>\n"
        "  important <id>       mark note <id> as important (red)\n"
        "  bold <id>            mark note <id> as bold (inverted)\n"
        "  blink <id>           mark note <id> as blinking yellow\n"
        "  normal <id>          remove any styling prefix from note <id>\n"
        "  delete <id>          delete note <id>\n"
        "  notebooks            list all notebooks\n"
        "  notebook <title>     switch to or create notebook <title>\n"
        "  search <query>       full-text search across all notes (FTS5 syntax)\n"
        "\n"
        "Text prefixes in a note: '!' = important (red), '*' = bold (inverted)."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("command", QCoreApplication::translate("main", "list | add | import | close | due | important | bold | blink | normal | delete | notebook | notebooks | search"));
    parser.addPositionalArgument("content", QCoreApplication::translate("main", "note id, note text, due date, notebook title or search query"));

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

    if(args.at(0) == "import")   {
        if(args.length() < 2) {
            out  << "no file to import...\n";
        }
        else {
            importNotes(query, db, args.at(1), currentNotebook);
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
        else if(args.length() < 3) {
            out  << "no due date provided, use yyyy-MM-dd (2016-12-31)\n";
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

    if(args.at(0) == "important")   {
        if(args.length() < 2) {
            out  << "no note to mark...\n";
        }
        else {
            markNote(query, db, args.at(1), '!');
        }
        exit(0);
    }

    if(args.at(0) == "bold")   {
        if(args.length() < 2) {
            out  << "no note to mark...\n";
        }
        else {
            markNote(query, db, args.at(1), '*');
        }
        exit(0);
    }

    if(args.at(0) == "blink")   {
        if(args.length() < 2) {
            out  << "no note to mark...\n";
        }
        else {
            markNote(query, db, args.at(1), '~');
        }
        exit(0);
    }

    if(args.at(0) == "normal")   {
        if(args.length() < 2) {
            out  << "no note to mark...\n";
        }
        else {
            markNote(query, db, args.at(1), QChar());
        }
        exit(0);
    }

    if(args.at(0) == "search")   {
        if(args.length() < 2) {
            out  << "no search query provided...\n";
        }
        else {
            search(query, db, args.mid(1).join(" "));
        }
        exit(0);
    }

    out << "unknown command: " << args.at(0) << "\nrun '" << QCoreApplication::applicationName() << " --help' for the list of commands\n";
    out.flush();
    return 1;
}
