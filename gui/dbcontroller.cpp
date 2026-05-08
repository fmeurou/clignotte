#include "dbcontroller.h"

#include "notebookmodel.h"
#include "notemodel.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QUuid>

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
