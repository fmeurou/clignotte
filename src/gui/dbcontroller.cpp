#include "dbcontroller.h"

#include "notebookmodel.h"
#include "notemodel.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QSet>
#include <QSettings>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QThread>
#include <QUuid>

namespace {

struct SyncNote {
    QString uuid, notebook, text;
    QDateTime createdAt, doneAt;
    QDate dueDate;
    QStringList tags;
};

static QString serializeSyncNote(const SyncNote &n)
{
    QString s;
    s += "uuid: "       + n.uuid + "\n";
    s += "notebook: "   + n.notebook + "\n";
    s += "created_at: " + (n.createdAt.isValid() ? n.createdAt.toString(Qt::ISODateWithMs) : QString()) + "\n";
    s += "due_date: "   + (n.dueDate.isValid()   ? n.dueDate.toString(Qt::ISODate)         : QString()) + "\n";
    s += "done_at: "    + (n.doneAt.isValid()     ? n.doneAt.toString(Qt::ISODateWithMs)    : QString()) + "\n";
    s += "tags: "       + n.tags.join(", ") + "\n\n";
    s += n.text;
    if (!s.endsWith('\n')) s += "\n";
    return s;
}

static bool parseSyncNote(const QString &content, SyncNote &n)
{
    int sep = content.indexOf("\n\n");
    if (sep < 0) return false;
    n.text = content.mid(sep + 2);
    if (n.text.endsWith('\n')) n.text.chop(1);
    for (const QString &line : content.left(sep).split('\n')) {
        int colon = line.indexOf(':');
        if (colon < 0) continue;
        QString key = line.left(colon).trimmed();
        QString val = line.mid(colon + 1).trimmed();
        if      (key == "uuid")       n.uuid      = val;
        else if (key == "notebook")   n.notebook  = val;
        else if (key == "created_at") n.createdAt = QDateTime::fromString(val, Qt::ISODateWithMs);
        else if (key == "due_date")   n.dueDate   = val.isEmpty() ? QDate()     : QDate::fromString(val, Qt::ISODate);
        else if (key == "done_at")    n.doneAt    = val.isEmpty() ? QDateTime() : QDateTime::fromString(val, Qt::ISODateWithMs);
        else if (key == "tags") {
            n.tags.clear();
            for (const QString &t : val.split(',', Qt::SkipEmptyParts)) {
                QString tt = t.trimmed();
                if (!tt.isEmpty()) n.tags << tt;
            }
        }
    }
    return !n.uuid.isEmpty();
}

static QString exportToDirImpl(QSqlDatabase &db, const QString &dirPath)
{
    QDir d(dirPath);
    if (!d.exists() && !d.mkpath("."))
        return "Cannot create directory: " + dirPath;

    QSqlQuery q(db);
    if (!q.exec("SELECT note.id, note.uuid, note.created_at, note.due_date, note.done_at, "
                "note.text, notebook.title "
                "FROM note INNER JOIN notebook ON note.notebook=notebook.id"))
        return "Export query failed";

    QSet<QString> written;
    int count = 0;
    db.transaction();
    while (q.next()) {
        SyncNote n;
        int id   = q.value(0).toInt();
        n.uuid   = q.value(1).toString();
        if (n.uuid.isEmpty()) {
            n.uuid = QUuid::createUuid().toString(QUuid::WithoutBraces);
            QSqlQuery uq(db);
            uq.prepare("UPDATE note SET uuid=:u WHERE id=:id");
            uq.bindValue(":u",   n.uuid);
            uq.bindValue(":id",  id);
            uq.exec();
        }
        n.createdAt = q.value(2).toDateTime();
        n.dueDate   = q.value(3).toDate();
        n.doneAt    = q.value(4).toDateTime();
        n.text      = q.value(5).toString();
        n.notebook  = q.value(6).toString();
        QSqlQuery tq(db);
        tq.prepare("SELECT tag FROM note_tag WHERE note=:n ORDER BY tag");
        tq.bindValue(":n", id);
        if (tq.exec()) while (tq.next()) n.tags << tq.value(0).toString();

        QFile f(d.filePath(n.uuid + ".md"));
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            f.write(serializeSyncNote(n).toUtf8());
            written.insert(n.uuid);
            count++;
        }
    }
    db.commit();

    int pruned = 0;
    for (const QString &name : d.entryList({"*.md"}, QDir::Files)) {
        if (!written.contains(name.left(name.length() - 3))) {
            d.remove(name);
            pruned++;
        }
    }
    QString msg = QString("exported %1 note(s)").arg(count);
    if (pruned) msg += QString(", pruned %1").arg(pruned);
    return msg;
}

static QString syncFromDirImpl(QSqlDatabase &db, const QString &dirPath)
{
    QDir d(dirPath);
    if (!d.exists()) return "Directory not found: " + dirPath;

    int created = 0, updated = 0, unchanged = 0, skipped = 0;
    db.transaction();
    for (const QString &name : d.entryList({"*.md"}, QDir::Files)) {
        QFile f(d.filePath(name));
        if (!f.open(QIODevice::ReadOnly)) { skipped++; continue; }
        SyncNote n;
        if (!parseSyncNote(QString::fromUtf8(f.readAll()), n)) { skipped++; continue; }

        QSqlQuery nq(db);
        nq.prepare("SELECT id FROM notebook WHERE title=:t");
        nq.bindValue(":t", n.notebook);
        nq.exec();
        int nbId;
        if (nq.first()) {
            nbId = nq.value(0).toInt();
        } else {
            QSqlQuery ins(db);
            ins.prepare("INSERT INTO notebook(title, last_used) VALUES(:t, 0)");
            ins.bindValue(":t", n.notebook);
            ins.exec();
            nbId = ins.lastInsertId().toInt();
        }

        QSqlQuery eq(db);
        eq.prepare("SELECT id, created_at, due_date, done_at, text, notebook FROM note WHERE uuid=:u");
        eq.bindValue(":u", n.uuid);
        eq.exec();
        int noteId;
        if (eq.first()) {
            noteId = eq.value(0).toInt();
            bool same = eq.value(1).toDateTime() == n.createdAt
                     && eq.value(2).toDate()     == n.dueDate
                     && eq.value(3).toDateTime() == n.doneAt
                     && eq.value(4).toString()   == n.text
                     && eq.value(5).toInt()      == nbId;
            if (!same) {
                QSqlQuery up(db);
                up.prepare("UPDATE note SET notebook=:nb, created_at=:ca, due_date=:dd, "
                           "done_at=:da, text=:t WHERE id=:id");
                up.bindValue(":nb",  nbId);
                up.bindValue(":ca",  n.createdAt);
                up.bindValue(":dd",  n.dueDate.isValid() ? QVariant(n.dueDate) : QVariant());
                up.bindValue(":da",  n.doneAt.isValid()  ? QVariant(n.doneAt)  : QVariant());
                up.bindValue(":t",   n.text);
                up.bindValue(":id",  noteId);
                up.exec();
                updated++;
            } else {
                unchanged++;
            }
        } else {
            QSqlQuery ins(db);
            ins.prepare("INSERT INTO note(notebook, uuid, created_at, due_date, done_at, text) "
                        "VALUES(:nb,:u,:ca,:dd,:da,:t)");
            ins.bindValue(":nb", nbId);
            ins.bindValue(":u",  n.uuid);
            ins.bindValue(":ca", n.createdAt);
            ins.bindValue(":dd", n.dueDate.isValid() ? QVariant(n.dueDate) : QVariant());
            ins.bindValue(":da", n.doneAt.isValid()  ? QVariant(n.doneAt)  : QVariant());
            ins.bindValue(":t",  n.text);
            ins.exec();
            noteId = ins.lastInsertId().toInt();
            created++;
        }

        QSqlQuery dt(db);
        dt.prepare("DELETE FROM note_tag WHERE note=:n");
        dt.bindValue(":n", noteId);
        dt.exec();
        for (const QString &tag : n.tags) {
            QSqlQuery it(db);
            it.prepare("INSERT OR IGNORE INTO note_tag(note, tag) VALUES(:n, :t)");
            it.bindValue(":n", noteId);
            it.bindValue(":t", tag);
            it.exec();
        }
    }
    db.commit();

    QString msg = QString("sync: %1 created, %2 updated, %3 unchanged")
                      .arg(created).arg(updated).arg(unchanged);
    if (skipped) msg += QString(", %1 skipped").arg(skipped);
    return msg;
}

static QString runGitIn(const QString &dir)
{
    auto run = [&](QStringList args) -> QPair<int, QString> {
        QProcess p;
        p.setWorkingDirectory(dir);
        p.start("git", args);
        p.waitForFinished(60000);
        QString out = QString::fromUtf8(p.readAllStandardOutput()
                                        + p.readAllStandardError()).trimmed();
        return {p.exitCode(), out};
    };

    run({"add", "-A"});

    QStringList log;
    auto [commitCode, commitOut] = run({"commit", "-m", "clignotte sync"});
    if (commitCode == 0)
        log << "committed";

    auto [pullCode, pullOut] = run({"pull", "--rebase"});
    if (pullCode != 0)
        log << "pull failed: " + pullOut.left(120);
    else if (!pullOut.contains("up to date", Qt::CaseInsensitive))
        log << "pulled";

    auto [pushCode, pushOut] = run({"push"});
    if (pushCode != 0)
        log << "push failed: " + pushOut.left(120);
    else
        log << "pushed";

    return log.isEmpty() ? QString() : "git: " + log.join(", ");
}

} // namespace

DbController::DbController(QObject *parent) : QObject(parent) {}

DbController::~DbController()
{
    if (m_db.isOpen()) m_db.close();
}

bool DbController::initialize()
{
    QString storedNotes = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (!QDir(storedNotes).exists() && !QDir(storedNotes).mkpath(storedNotes)) {
        qCritical() << "unable to create app data dir" << storedNotes;
        return false;
    }
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName(QDir(storedNotes).absoluteFilePath("notes.db"));
    if (!m_db.open()) {
        qCritical() << "open failed:" << m_db.lastError().text();
        return false;
    }
    ensureSchema();
    m_notebookModel = new NotebookModel(m_db, this);
    m_noteModel = new NoteModel(m_db, this);
    int cur = m_notebookModel->currentNotebookId();
    m_noteModel->setNotebookId(cur);
    return true;
}

void DbController::ensureSchema()
{
    QSqlQuery q(m_db);
    if (!m_db.tables().contains("notebook")) {
        q.exec("CREATE TABLE notebook(id INTEGER PRIMARY KEY, title TEXT, last_used BOOL)");
        q.exec("INSERT INTO notebook(id, title, last_used) values(0, 'default', 1)");
    }
    if (!m_db.tables().contains("note")) {
        q.exec("CREATE TABLE note(id INTEGER PRIMARY KEY, uuid TEXT, created_at DATETIME, "
               "due_date DATE, done_at DATETIME, text TEXT, notebook INTEGER, "
               "FOREIGN KEY(notebook) REFERENCES notebook(id))");
    }
    bool hasUuid = false;
    if (q.exec("PRAGMA table_info(note)")) {
        while (q.next()) if (q.value(1).toString() == "uuid") { hasUuid = true; break; }
    }
    if (!hasUuid) q.exec("ALTER TABLE note ADD COLUMN uuid TEXT");
    if (!m_db.tables().contains("attachment")) {
        q.exec("CREATE TABLE attachment(id INTEGER PRIMARY KEY, note INTEGER NOT NULL, "
               "path TEXT NOT NULL, original_path TEXT, created_at DATETIME, "
               "FOREIGN KEY(note) REFERENCES note(id))");
    }
    q.exec("CREATE TABLE IF NOT EXISTS note_tag(note INTEGER NOT NULL, tag TEXT NOT NULL COLLATE NOCASE, "
           "PRIMARY KEY(note, tag), FOREIGN KEY(note) REFERENCES note(id))");
    q.exec("CREATE INDEX IF NOT EXISTS note_tag_tag ON note_tag(tag)");
    q.exec("CREATE TRIGGER IF NOT EXISTS note_tag_ad AFTER DELETE ON note "
           "BEGIN DELETE FROM note_tag WHERE note = old.id; END");
}

int DbController::currentNotebookId() const
{
    return m_notebookModel ? m_notebookModel->currentNotebookId() : -1;
}

QString DbController::currentNotebookTitle() const
{
    return m_notebookModel ? m_notebookModel->currentNotebookTitle() : QString();
}

int DbController::notebookIdByTitle(const QString &title)
{
    QSqlQuery q(m_db);
    q.prepare("SELECT id FROM notebook WHERE title=:t");
    q.bindValue(":t", title);
    if (q.exec() && q.next()) return q.value(0).toInt();
    return -1;
}

int DbController::upsertNotebook(const QString &title)
{
    int id = notebookIdByTitle(title);
    if (id >= 0) return id;
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO notebook(title, last_used) VALUES(:t, 0)");
    q.bindValue(":t", title);
    if (!q.exec()) {
        qWarning() << "insert notebook failed" << q.lastError().text();
        return -1;
    }
    return q.lastInsertId().toInt();
}

void DbController::selectNotebook(int notebookId)
{
    QSqlQuery q(m_db);
    q.exec("UPDATE notebook SET last_used=0");
    q.prepare("UPDATE notebook SET last_used=1 WHERE id=:id");
    q.bindValue(":id", notebookId);
    q.exec();
    m_notebookModel->refresh();
    m_noteModel->setNotebookId(notebookId);
    m_noteModel->refresh();
    emit currentNotebookChanged();
}

void DbController::createNotebook(const QString &title)
{
    QString t = title.trimmed();
    if (t.isEmpty()) return;
    int id = upsertNotebook(t);
    if (id < 0) return;
    selectNotebook(id);
}

int DbController::addNote(const QString &text)
{
    QString t = text;
    while (t.endsWith('\n')) t.chop(1);
    if (t.trimmed().isEmpty()) return -1;
    int nbId = currentNotebookId();
    if (nbId < 0) return -1;
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO note(notebook, created_at, text, uuid) "
              "VALUES(:nb, :now, :text, :uuid)");
    q.bindValue(":nb", nbId);
    q.bindValue(":now", QDateTime::currentDateTime());
    q.bindValue(":text", t);
    q.bindValue(":uuid", QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!q.exec()) {
        qWarning() << "addNote failed:" << q.lastError().text();
        return -1;
    }
    int newId = q.lastInsertId().toInt();
    m_noteModel->refresh();
    emit notesChanged();
    return newId;
}

void DbController::updateNoteText(int noteId, const QString &text)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE note SET text=:t WHERE id=:id");
    q.bindValue(":t", text);
    q.bindValue(":id", noteId);
    if (!q.exec()) qWarning() << "updateNoteText:" << q.lastError().text();
    m_noteModel->refresh();
    emit notesChanged();
}

void DbController::deleteNote(int noteId)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM note WHERE id=:id");
    q.bindValue(":id", noteId);
    if (!q.exec()) qWarning() << "deleteNote:" << q.lastError().text();
    m_noteModel->refresh();
    emit notesChanged();
}

void DbController::markDone(int noteId)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE note SET done_at=:n WHERE id=:id");
    q.bindValue(":n", QDateTime::currentDateTime());
    q.bindValue(":id", noteId);
    if (!q.exec()) qWarning() << "markDone:" << q.lastError().text();
    m_noteModel->refresh();
    emit notesChanged();
}

void DbController::reopenNote(int noteId)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE note SET done_at=NULL WHERE id=:id");
    q.bindValue(":id", noteId);
    if (!q.exec()) qWarning() << "reopenNote:" << q.lastError().text();
    m_noteModel->refresh();
    emit notesChanged();
}

void DbController::setDueDate(int noteId, const QString &isoDate)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE note SET due_date=:d WHERE id=:id");
    q.bindValue(":d", isoDate);
    q.bindValue(":id", noteId);
    if (!q.exec()) qWarning() << "setDueDate:" << q.lastError().text();
    m_noteModel->refresh();
    emit notesChanged();
}

void DbController::clearDueDate(int noteId)
{
    QSqlQuery q(m_db);
    q.prepare("UPDATE note SET due_date=NULL WHERE id=:id");
    q.bindValue(":id", noteId);
    q.exec();
    m_noteModel->refresh();
    emit notesChanged();
}

void DbController::moveNote(int noteId, const QString &notebookTitle)
{
    int nbId = upsertNotebook(notebookTitle.trimmed());
    if (nbId < 0) return;
    QSqlQuery q(m_db);
    q.prepare("UPDATE note SET notebook=:nb WHERE id=:id");
    q.bindValue(":nb", nbId);
    q.bindValue(":id", noteId);
    q.exec();
    m_notebookModel->refresh();
    m_noteModel->refresh();
    emit notesChanged();
}

void DbController::tagNote(int noteId, const QString &tag)
{
    QString t = tag.trimmed();
    if (t.isEmpty()) return;
    QSqlQuery q(m_db);
    q.prepare("INSERT OR IGNORE INTO note_tag(note, tag) VALUES(:n, :t)");
    q.bindValue(":n", noteId);
    q.bindValue(":t", t);
    q.exec();
    m_noteModel->refresh();
    emit notesChanged();
}

void DbController::untagNote(int noteId, const QString &tag)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM note_tag WHERE note=:n AND tag=:t COLLATE NOCASE");
    q.bindValue(":n", noteId);
    q.bindValue(":t", tag);
    q.exec();
    m_noteModel->refresh();
    emit notesChanged();
}

QVariantList DbController::noteAttachments(int noteId) const
{
    QVariantList out;
    QSqlQuery q(m_db);
    q.prepare("SELECT id, path, original_path FROM attachment WHERE note=:n ORDER BY id");
    q.bindValue(":n", noteId);
    if (q.exec()) {
        while (q.next()) {
            QVariantMap m;
            m["id"]           = q.value(0).toInt();
            m["path"]         = q.value(1).toString();
            m["originalPath"] = q.value(2).toString();
            out << m;
        }
    }
    return out;
}

void DbController::attachFile(int noteId, const QString &path)
{
    QSqlQuery q(m_db);
    q.prepare("INSERT INTO attachment(note, path, original_path, created_at) "
              "VALUES(:n, :p, :op, :now)");
    q.bindValue(":n",   noteId);
    q.bindValue(":p",   path);
    q.bindValue(":op",  path);
    q.bindValue(":now", QDateTime::currentDateTime());
    if (!q.exec()) qWarning() << "attachFile:" << q.lastError().text();
    m_noteModel->refresh();
    emit notesChanged();
    emit attachmentsChanged();
}

void DbController::detachFile(int attachmentId)
{
    QSqlQuery q(m_db);
    q.prepare("DELETE FROM attachment WHERE id=:id");
    q.bindValue(":id", attachmentId);
    if (!q.exec()) qWarning() << "detachFile:" << q.lastError().text();
    m_noteModel->refresh();
    emit notesChanged();
    emit attachmentsChanged();
}

QStringList DbController::allTags() const
{
    QStringList out;
    QSqlQuery q(m_db);
    if (q.exec("SELECT tag FROM note_tag GROUP BY tag ORDER BY tag COLLATE NOCASE")) {
        while (q.next()) out << q.value(0).toString();
    }
    return out;
}

QString DbController::syncDir() const
{
    QSettings s("clignotte", "clignotte-gui");
    return s.value("syncDir").toString();
}

void DbController::setSyncDir(const QString &dir)
{
    QSettings s("clignotte", "clignotte-gui");
    s.setValue("syncDir", dir);
    emit syncDirChanged();
}

void DbController::runSync()
{
    QString dir = syncDir();
    if (dir.isEmpty()) {
        emit syncFinished("No sync directory configured");
        return;
    }
    emit syncStarted();

    QString dbPath = m_db.databaseName();
    static QAtomicInt connCounter{0};
    QString connName = "sync_" + QString::number(connCounter.fetchAndAddRelaxed(1));

    QThread *t = QThread::create([this, dir, dbPath, connName]() {
        QStringList log;
        {
            QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE", connName);
            db.setDatabaseName(dbPath);
            if (db.open()) {
                log << exportToDirImpl(db, dir);
                if (QDir(dir + "/.git").exists())
                    log << runGitIn(dir);
                log << syncFromDirImpl(db, dir);
                db.close();
            } else {
                log << "Cannot open database in sync thread";
            }
        }
        QSqlDatabase::removeDatabase(connName);

        QMetaObject::invokeMethod(this, [this, log]() {
            m_notebookModel->refresh();
            m_noteModel->refresh();
            emit notesChanged();
            emit syncFinished(log.join("\n"));
        }, Qt::QueuedConnection);
    });
    t->start();
    connect(t, &QThread::finished, t, &QThread::deleteLater);
}
