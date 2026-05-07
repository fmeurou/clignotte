#include "attachments.h"
#include "common.h"
#include "completions.h"
#include "db.h"
#include "import.h"
#include "notebook.h"
#include "notes.h"
#include "sync.h"
#include "tags.h"

#include <QChar>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <QMap>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QString>
#include <QStringList>
#include <QTextStream>
#include <iostream>

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
    auto toolStatus = [](const QString &name) -> QString {
        return QStandardPaths::findExecutable(name).isEmpty() ? "[not found]" : "[found]";
    };

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main",
        "clignotte - command-line note keeper.\n"
        "\n"
        "Commands:\n"
        "  (none)               display notes grouped by notebook\n"
        "  list                 display notes in a table\n"
        "  today                open dated notes due today or overdue, across all notebooks\n"
        "  week                 open dated notes due in the next 7 days, across all notebooks\n"
        "  bytag                display notes grouped by tag\n"
        "  add <text>           add a note to the current notebook\n"
        "  edit <id>            open $EDITOR to edit note <id>\n"
        "  move <id> <title>    move note <id> to notebook <title> (created if absent)\n"
        "  import <path>        import from text file, audio (whisper), or image/PDF (tesseract)\n"
        "  close <id>           mark note <id> as done\n"
        "  due <id> <date>      set due date (yyyy-MM-dd) for note <id>\n"
        "  important <id>       mark note <id> as important (red)\n"
        "  bold <id>            mark note <id> as bold (inverted)\n"
        "  blink <id>           mark note <id> as blinking yellow\n"
        "  normal <id>          remove any styling prefix from note <id>\n"
        "  delete <id>          delete note <id>\n"
        "  notebooks            list all notebooks\n"
        "  notebook <title>     switch to or create notebook <title>\n"
        "  close-notebook <title>  mark every open note in notebook <title> as done\n"
        "  search <query>       full-text search across all notes (FTS5 syntax)\n"
        "  attach <id> <path>   attach a local file or URL to note <id>\n"
        "  attachments <id>     list attachments for note <id>\n"
        "  detach <attach_id>   remove an attachment by its id\n"
        "  tag <id> <name>...   add one or more tags to note <id>\n"
        "  untag <id> <name>... remove tags from note <id>\n"
        "  tags                 list all tags with usage counts\n"
        "  tags <id>            list tags on note <id>\n"
        "  export <dir>         write all notes to <dir> as one .md per note (for git sync)\n"
        "  sync <dir>           reconcile notes from <dir> into the database\n"
        "  completions <shell>  emit shell completion script (bash, zsh, fish)\n"
        "\n"
        "Options for 'attach':\n"
        "  --copy               copy the local file into the clignotte storage folder\n"
        "\n"
        "Options for 'sync':\n"
        "  --delete             also delete local notes whose files are absent (mirror semantics)\n"
        "\n"
        "Stdin / piping:\n"
        "  add -                read note text from stdin (single note)\n"
        "  import -             import notes from stdin (one note per non-empty line)\n"
        "  --json               machine-readable output for list/today/week/search/(no command)\n"
        "  --tag <name>         filter list/today/week/search/bytag by tag (repeatable; AND)\n"
        "  --notebook <title>   filter list/today/week/search/bytag by notebook (exact, case-sensitive)\n"
        "  --all                include closed notes in list/today/week/search/bytag/(no command)\n"
        "  NO_COLOR=1           suppress ANSI styling (auto-disabled when stdout isn't a tty)\n"
        "\n"
        "Optional tools:\n"
        "  whisper    %1  audio import (pacman -S python-openai-whisper)\n"
        "  tesseract  %2  image/PDF OCR import (pacman -S tesseract tesseract-data-eng)\n"
        "  pdftoppm   %3  PDF-to-image conversion for OCR (pacman -S poppler)\n"
        "\n"
        "Text prefixes in a note: '!' = important (red), '*' = bold (inverted).")
        .arg(toolStatus("whisper"), toolStatus("tesseract"), toolStatus("pdftoppm")));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption copyOption("copy", "Copy the local file into the clignotte storage folder (used with attach)");
    parser.addOption(copyOption);
    QCommandLineOption jsonOption("json", "Emit machine-readable JSON output (list, today, week, search, no command)");
    parser.addOption(jsonOption);
    QCommandLineOption tagOption(QStringList() << "tag", "Filter listings by tag (repeatable; AND semantics)", "name");
    parser.addOption(tagOption);
    QCommandLineOption notebookOption(QStringList() << "notebook", "Filter listings by notebook (exact title, case-sensitive)", "title");
    parser.addOption(notebookOption);
    QCommandLineOption deleteOption("delete", "With sync: delete local notes whose files are absent (mirror semantics)");
    parser.addOption(deleteOption);
    QCommandLineOption allOption("all", "Include closed notes in list/today/week/search/bytag/(no command)");
    parser.addOption(allOption);
    parser.addPositionalArgument("command", QCoreApplication::translate("main", "list | today | week | bytag | add | edit | move | import | close | close-notebook | due | important | bold | blink | normal | delete | notebook | notebooks | search | attach | attachments | detach | tag | untag | tags | export | sync | completions"));
    parser.addPositionalArgument("content", QCoreApplication::translate("main", "note id, note text, due date, notebook title or search query"));

    parser.process(app);

    const QStringList args = parser.positionalArguments();
    const bool jsonOut = parser.isSet(jsonOption);
    const bool includeClosed = parser.isSet(allOption);
    const QStringList tagFilter = parser.values(tagOption);
    const QString notebookFilter = parser.value(notebookOption);
    if(!args.length())  {
        if(jsonOut || !tagFilter.isEmpty() || !notebookFilter.isEmpty())
            list(query, db, jsonOut, includeClosed, tagFilter, notebookFilter);
        else
            pretty(query, db, includeClosed);
        exit(0);
    }
    if(args.at(0) == "list")  {
        list(query, db, jsonOut, includeClosed, tagFilter, notebookFilter);
        exit(0);
    }
    if(args.at(0) == "today")  {
        agenda(query, db, 0, "No notes due today\n", jsonOut, includeClosed, tagFilter, notebookFilter);
        exit(0);
    }
    if(args.at(0) == "week")  {
        agenda(query, db, 6, "No notes due in the next 7 days\n", jsonOut, includeClosed, tagFilter, notebookFilter);
        exit(0);
    }
    if(args.at(0) == "bytag")  {
        byTag(query, db, jsonOut, includeClosed, notebookFilter);
        exit(0);
    }
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

    if(args.at(0) == "close-notebook")   {
        if(args.length() < 2) {
            out  << "usage: close-notebook <title>\n";
        }
        else {
            closeNotebook(query, db, args.mid(1).join(" "));
        }
        exit(0);
    }

    if(args.at(0) == "add")   {
        if(args.length() < 2) {
            out  << "no note to add...";
        }
        else if(args.length() == 2 && args.at(1) == "-") {
            QTextStream in(stdin);
            QString text = in.readAll();
            while(text.endsWith('\n')) text.chop(1);
            if(text.trimmed().isEmpty()) {
                out << "empty stdin, nothing to add\n";
            } else {
                addNote(query, db, text, currentNotebook);
            }
        }
        else {
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

    if(args.at(0) == "edit")   {
        if(args.length() < 2) {
            out  << "no note to edit...\n";
        }
        else {
            editNote(query, db, args.at(1));
        }
        exit(0);
    }

    if(args.at(0) == "move")   {
        if(args.length() < 3) {
            out  << "usage: move <note_id> <notebook>\n";
        }
        else {
            moveNote(query, db, args.at(1), args.mid(2).join(" "));
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
            search(query, db, args.mid(1).join(" "), jsonOut, includeClosed, tagFilter, notebookFilter);
        }
        exit(0);
    }

    if(args.at(0) == "attach")   {
        if(args.length() < 3) {
            out  << "usage: attach <note_id> <path_or_url> [--copy]\n";
        }
        else {
            attachFile(query, db, args.at(1), args.at(2), parser.isSet(copyOption), storedNotes);
        }
        exit(0);
    }

    if(args.at(0) == "attachments")   {
        if(args.length() < 2) {
            out  << "no note id provided...\n";
        }
        else {
            listAttachments(query, db, args.at(1));
        }
        exit(0);
    }

    if(args.at(0) == "detach")   {
        if(args.length() < 2) {
            out  << "no attachment id provided...\n";
        }
        else {
            detachFile(query, db, args.at(1));
        }
        exit(0);
    }

    if(args.at(0) == "tag")   {
        if(args.length() < 3) {
            out  << "usage: tag <note_id> <name> [<name>...]\n";
        }
        else {
            tagNote(query, db, args.at(1), args.mid(2));
        }
        exit(0);
    }

    if(args.at(0) == "untag")   {
        if(args.length() < 3) {
            out  << "usage: untag <note_id> <name> [<name>...]\n";
        }
        else {
            untagNote(query, db, args.at(1), args.mid(2));
        }
        exit(0);
    }

    if(args.at(0) == "tags")   {
        if(args.length() < 2) {
            listAllTags(query, db);
        }
        else {
            listNoteTags(query, db, args.at(1));
        }
        exit(0);
    }

    if(args.at(0) == "export")   {
        if(args.length() < 2) {
            out  << "usage: export <directory>\n";
        }
        else {
            exportNotes(query, db, args.at(1));
        }
        exit(0);
    }

    if(args.at(0) == "sync")   {
        if(args.length() < 2) {
            out  << "usage: sync <directory> [--delete]\n";
        }
        else {
            syncNotes(db, args.at(1), parser.isSet(deleteOption));
        }
        exit(0);
    }

    if(args.at(0) == "completions")   {
        if(args.length() < 2) {
            out << "usage: completions <bash|zsh|fish>\n";
        }
        else {
            writeCompletions(args.at(1));
        }
        exit(0);
    }

    out << "unknown command: " << args.at(0) << "\nrun '" << QCoreApplication::applicationName() << " --help' for the list of commands\n";
    out.flush();
    return 1;
}
