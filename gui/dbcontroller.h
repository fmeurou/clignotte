#pragma once

#include "notebookmodel.h"
#include "notemodel.h"

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVariantList>

class DbController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(NotebookModel* notebooks READ notebooks CONSTANT)
    Q_PROPERTY(NoteModel* notes READ notes CONSTANT)
    Q_PROPERTY(int currentNotebookId READ currentNotebookId NOTIFY currentNotebookChanged)
    Q_PROPERTY(QString currentNotebookTitle READ currentNotebookTitle NOTIFY currentNotebookChanged)
public:
    explicit DbController(QObject *parent = nullptr);
    ~DbController() override;

    bool initialize();

    NotebookModel *notebooks() const { return m_notebookModel; }
    NoteModel *notes() const { return m_noteModel; }
    int currentNotebookId() const;
    QString currentNotebookTitle() const;

    Q_INVOKABLE void selectNotebook(int notebookId);
    Q_INVOKABLE void createNotebook(const QString &title);
    Q_INVOKABLE int  addNote(const QString &text);
    Q_INVOKABLE void updateNoteText(int noteId, const QString &text);
    Q_INVOKABLE void deleteNote(int noteId);
    Q_INVOKABLE void markDone(int noteId);
    Q_INVOKABLE void reopenNote(int noteId);
    Q_INVOKABLE void setDueDate(int noteId, const QString &isoDate);
    Q_INVOKABLE void clearDueDate(int noteId);
    Q_INVOKABLE void moveNote(int noteId, const QString &notebookTitle);
    Q_INVOKABLE void tagNote(int noteId, const QString &tag);
    Q_INVOKABLE void untagNote(int noteId, const QString &tag);
    Q_INVOKABLE QStringList allTags() const;
    Q_INVOKABLE QVariantList noteAttachments(int noteId) const;
    Q_INVOKABLE void attachFile(int noteId, const QString &path);
    Q_INVOKABLE void detachFile(int attachmentId);

signals:
    void currentNotebookChanged();
    void notesChanged();
    void attachmentsChanged();

private:
    QSqlDatabase m_db;
    NotebookModel *m_notebookModel = nullptr;
    NoteModel *m_noteModel = nullptr;

    void ensureSchema();
    int notebookIdByTitle(const QString &title);
    int upsertNotebook(const QString &title);
};
