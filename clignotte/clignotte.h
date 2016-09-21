#ifndef CLIGNOTTE_H
#define CLIGNOTTE_H

#include "clignotte_global.h"

// DEFINE queries
#define CREATE_NOTEBOOK_TABLE "CREATE TABLE notebook(id INTEGER PRIMARY KEY, title TEXT, last_used BOOL);"
#define CREATE_NOTE_TABLE "CREATE TABLE note(id INTEGER PRIMARY KEY, created_at DATETIME, due_date DATE, done_at DATETIME, text TEXT, notebook INTEGER, FOREIGN KEY(notebook) REFERENCES notebook(id));"
#define INIT_NOTEBOOK_TABLE "INSERT INTO notebook(id, title, last_used) values(0, 'default', 1);"
#define LIST_NOTES "SELECT notebook.title, note.id, note.due_date, note.done_at, note.text FROM note INNER JOIN notebook ON note.notebook=notebook.id ORDER BY done_at, due_date ASC, text"
#define LIST_ACTIVE_NOTEBOOKS "SELECT DISTINCT notebook.id, notebook.title FROM note INNER JOIN notebook ON note.notebook=notebook.id ORDER BY notebook.title"
#define LIST_NOTEBOOK_NOTES "select note.text, note.due_date, note.done_at FROM note INNER JOIN notebook ON note.notebook=notebook.id WHERE notebook.id=:notebook ORDER BY done_at, due_date ASC, text"
#define CURRENT_NOTEBOOK "SELECT id, title FROM notebook WHERE last_used = 1"
#define INSERT_NOTE "insert into note(notebook, created_at, text) values(:notebook, :currentDateTime, :text)"
#define UPDATE_DUE_DATE "UPDATE note SET due_date=:dueDate WHERE id=:id"
#define UPDATE_DONE "UPDATE note SET done_at=:currentDateTime WHERE id=:id"
#define DELETE_NOTE "delete from note where id=:id"
#define SELECT_NOTEBOOK_BY_TITLE "SELECT id, title FROM notebook WHERE title=:title"
#define RESET_LAST_USED "UPDATE notebook SET last_used=0"
#define UPDATE_LAST_USED "UPDATE notebook SET last_used=1 WHERE id=%1"
#define INSERT_NOTEBOOK "insert into notebook values(NULL, :title, 1);"
#define LIST_NOTEBOOKS "SELECT title, last_used FROM notebook ORDER BY last_used, title"

#define IMPORTANT_TEXT "\e[1;31m"
#define URGENT_TEXT "\e[7;31m"
#define BOLD_TEXT "\e[1m"
#define ITALIC_TEXT "\e[3m"
#define END_BOLD_TEXT "\e[21m"
#define INVERTED_TEXT "\e[7m"
#define END_INVERTED_TEXT "\e[27m"
#define NORMAL_TEXT "\e[0m"
#define UNDERLINED_TEXT "\e[4m"


class CLIGNOTTESHARED_EXPORT Clignotte
{

public:
    Clignotte();
};

#endif // CLIGNOTTE_H
