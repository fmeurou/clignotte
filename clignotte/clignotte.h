#ifndef CLIGNOTTE_H
#define CLIGNOTTE_H

#include "clignotte_global.h"
#include <QObject>
#include <QSqlDatabase>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QDebug>
#include <QTextStream>
#include <QDateTime>
#include <QDir>

// DEFINE queries
// DDL
#define CREATE_NOTEBOOK_TABLE "CREATE TABLE notebook(id INTEGER PRIMARY KEY, title TEXT, last_used BOOL);"
#define CREATE_NOTE_TABLE "CREATE TABLE note(id INTEGER PRIMARY KEY, created_at DATETIME, due_date DATE, done_at DATETIME, text TEXT, notebook INTEGER, FOREIGN KEY(notebook) REFERENCES notebook(id));"
// Notebooks
#define INIT_NOTEBOOK_TABLE "INSERT INTO notebook(id, title, last_used) values(0, 'default', 1);"
#define CURRENT_NOTEBOOK "SELECT id, title, last_used FROM notebook WHERE last_used = 1"
#define GET_NOTEBOOK_BY_TITLE "SELECT id, title, last_used FROM notebook WHERE title=:title"
#define GET_NOTEBOOK_BY_ID  "SELECT id, title, last_used FROM notebook WHERE id=:id"
#define RESET_LAST_USED "UPDATE notebook SET last_used=0"
#define UPDATE_LAST_USED "UPDATE notebook SET last_used=1 WHERE id=%1"
#define INSERT_NOTEBOOK "insert into notebook values(NULL, :title, 1);"
#define LIST_NOTEBOOKS "SELECT id, title, last_used FROM notebook ORDER BY last_used, title"
// Notes
#define LIST_NOTES "SELECT notebook.title, note.id, note.created_at, note.due_date, note.done_at, note.text FROM note INNER JOIN notebook ON note.notebook=notebook.id ORDER BY done_at, due_date ASC, text"
#define LIST_ACTIVE_NOTEBOOKS "SELECT DISTINCT notebook.id, notebook.title, notebook.last_used FROM note INNER JOIN notebook ON note.notebook=notebook.id ORDER BY notebook.title"
#define LIST_NOTEBOOK_NOTES "SELECT notebook.title, note.id, note.created_at, note.due_date, note.done_at, note.text FROM note INNER JOIN notebook ON note.notebook=notebook.id WHERE notebook.id=:notebook ORDER BY done_at, due_date ASC, text"
#define INSERT_NOTE "insert into note(notebook, created_at, text) values(:notebook, :currentDateTime, :text)"
#define UPDATE_DUE_DATE "UPDATE note SET due_date=:dueDate WHERE id=:id"
#define UPDATE_DONE "UPDATE note SET done_at=:currentDateTime WHERE id=:id"
#define DELETE_NOTE "DELETE FROM note WHERE id=:id"
#define GET_NOTE_BY_ID "SELECT notebook.title, note.id, note.created_at, note.due_date, note.done_at, note.text FROM note INNER JOIN notebook ON note.notebook=notebook.id WHERE note.id=:id"
// output styles
#define IMPORTANT_TEXT "\e[1;31m"
#define URGENT_TEXT "\e[7;31m"
#define BOLD_TEXT "\e[1m"
#define ITALIC_TEXT "\e[3m"
#define END_BOLD_TEXT "\e[21m"
#define INVERTED_TEXT "\e[7m"
#define END_INVERTED_TEXT "\e[27m"
#define NORMAL_TEXT "\e[0m"
#define UNDERLINED_TEXT "\e[4m"

class CLIGNOTTESHARED_EXPORT Note;

class CLIGNOTTESHARED_EXPORT Notebook
{
public:
    Notebook(QSqlDatabase);
    int getId();
    bool getLastUsed();
    QString getTitle();
    int save(); // 0: error, 1: updated, 2: created
    static Notebook get(QSqlDatabase, int);
    static Notebook get(QSqlDatabase, QString);
    static Notebook create(QSqlDatabase, QSqlRecord);
    void setLastUsed(bool);
    void setId(int);
    void setTitle(QString);
    QList<Note> notes();
    int addNote(QString text);
    inline QSqlDatabase getDb() {return m_db;}

private:
    int m_id;
    QString m_title;
    bool m_lastUsed;
    QSqlDatabase m_db;
};

class CLIGNOTTESHARED_EXPORT Note
{
public:
    Note(QSqlDatabase);
    static Note create(QSqlDatabase, QSqlRecord);
    bool save();
    bool updateDueDate(QDate);
    static Note get(int);
    bool isImportant();
    bool isBold();
    bool isDue();
    bool isDone();
    bool setDone();
    bool remove();
    inline int getId() {return m_id;}
    inline QString getText() {return m_text;}
    inline QDate getDueDate() {return m_dueDate;}
    inline QDateTime getCreatedAt() {return m_createdAt;}
    inline QDateTime getDone() {return m_doneDate;}
    inline QSqlDatabase getDb() {return m_db;}
    inline int getNotebookId() {return m_notebookId;}
    inline void setId(int v_id) {m_id = v_id;}
    inline QString setText(QString v_text) {m_text = v_text;}
    inline void setCreatedAt(QDateTime v_createdAt) {m_createdAt = v_createdAt;}
    inline void setDoneDate(QDateTime v_doneDate) {m_doneDate = v_doneDate;}
    inline void setDueDate(QDate v_dueDate) {m_dueDate = v_dueDate;}
    inline void setNotebookId(int v_notebookId) {m_notebookId = v_notebookId;}
    Notebook getNotebook();
    static Note get(QSqlDatabase, int);

private:
    int m_id;
    QString m_text;
    QDateTime   m_createdAt, m_doneDate;
    QDate m_dueDate;
    QSqlDatabase m_db;
    int m_notebookId;
};



class CLIGNOTTESHARED_EXPORT Clignotte
{

public:
    Clignotte(QString);
    bool initDb();
    QList<Notebook> notebooks(const int);
    QList<Notebook> activeNotebooks();
    QList<Note> notes();
    Notebook currentNotebook();
    Notebook getNotebook(int);
    Notebook getNotebook(QString);
    int createNotebook(QString);
    Note getNote(int);
    int createNote(QString);
    inline QString getLocation() {return m_location;}
    static const int NoFilter = 0;
    static const int Active = 1;
    static const int Current = 2;


private:
    QSqlDatabase m_db;
    QString m_location;

};




#endif // CLIGNOTTE_H
