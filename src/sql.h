#pragma once

// schema bootstrap
#define CREATE_NOTEBOOK_TABLE   "CREATE TABLE notebook(id INTEGER PRIMARY KEY, title TEXT, last_used BOOL);"
#define CREATE_NOTE_TABLE       "CREATE TABLE note(id INTEGER PRIMARY KEY, uuid TEXT, created_at DATETIME, due_date DATE, done_at DATETIME, text TEXT, notebook INTEGER, FOREIGN KEY(notebook) REFERENCES notebook(id));"
#define INIT_NOTEBOOK_TABLE     "INSERT INTO notebook(id, title, last_used) values(0, 'default', 1);"
#define CREATE_ATTACHMENT_TABLE "CREATE TABLE attachment(id INTEGER PRIMARY KEY, note INTEGER NOT NULL, path TEXT NOT NULL, original_path TEXT, created_at DATETIME, FOREIGN KEY(note) REFERENCES note(id));"

// FTS bootstrap
#define CREATE_NOTE_FTS_TABLE   "CREATE VIRTUAL TABLE IF NOT EXISTS note_fts USING fts5(text, content='note', content_rowid='id')"
#define CREATE_FTS_TRIGGER_AI   "CREATE TRIGGER IF NOT EXISTS note_ai AFTER INSERT ON note BEGIN INSERT INTO note_fts(rowid, text) VALUES (new.id, new.text); END"
#define CREATE_FTS_TRIGGER_AD   "CREATE TRIGGER IF NOT EXISTS note_ad AFTER DELETE ON note BEGIN INSERT INTO note_fts(note_fts, rowid, text) VALUES('delete', old.id, old.text); END"
#define CREATE_FTS_TRIGGER_AU   "CREATE TRIGGER IF NOT EXISTS note_au AFTER UPDATE ON note BEGIN INSERT INTO note_fts(note_fts, rowid, text) VALUES('delete', old.id, old.text); INSERT INTO note_fts(rowid, text) VALUES (new.id, new.text); END"
#define BACKFILL_NOTE_FTS       "INSERT INTO note_fts(rowid, text) SELECT id, text FROM note"

// notes — list/search default to open notes; the dynamic path drops the
// done_at filter when --all is set
#define LIST_NOTES              "SELECT notebook.title, note.id, note.due_date, note.done_at, note.text, (SELECT COUNT(*) FROM attachment WHERE attachment.note=note.id) FROM note INNER JOIN notebook ON note.notebook=notebook.id WHERE note.done_at IS NULL ORDER BY note.id"
#define INSERT_NOTE             "insert into note(notebook, created_at, text, uuid) values(:notebook, :currentDateTime, :text, :uuid)"
#define UPDATE_DUE_DATE         "UPDATE note SET due_date=:dueDate WHERE id=:id"
#define UPDATE_DONE             "UPDATE note SET done_at=:currentDateTime WHERE id=:id"
#define SELECT_NOTE_TEXT        "SELECT text FROM note WHERE id=:id"
#define UPDATE_NOTE_TEXT        "UPDATE note SET text=:text WHERE id=:id"
#define UPDATE_NOTE_NOTEBOOK    "UPDATE note SET notebook=:notebook WHERE id=:id"
#define DELETE_NOTE             "delete from note where id=:id"
#define SEARCH_NOTES            "SELECT notebook.title, note.id, note.due_date, note.done_at, note.text, (SELECT COUNT(*) FROM attachment WHERE attachment.note=note.id) FROM note_fts INNER JOIN note ON note.id=note_fts.rowid INNER JOIN notebook ON note.notebook=notebook.id WHERE note_fts MATCH :q AND note.done_at IS NULL ORDER BY rank"
#define LIST_AGENDA             "SELECT notebook.title, note.id, note.due_date, note.done_at, note.text, (SELECT COUNT(*) FROM attachment WHERE attachment.note=note.id) FROM note INNER JOIN notebook ON note.notebook=notebook.id WHERE note.due_date IS NOT NULL AND note.due_date <= :until AND note.done_at IS NULL ORDER BY note.due_date, note.id"
#define CLOSE_NOTEBOOK_NOTES    "UPDATE note SET done_at=:now WHERE notebook=:notebook AND done_at IS NULL"

// notebooks — pretty's no-arg view filters out notebooks whose notes are all closed
#define LIST_ACTIVE_NOTEBOOKS   "SELECT DISTINCT notebook.id, notebook.title FROM note INNER JOIN notebook ON note.notebook=notebook.id WHERE note.done_at IS NULL ORDER BY notebook.title"
#define LIST_ALL_NOTEBOOKS_WITH_NOTES "SELECT DISTINCT notebook.id, notebook.title FROM note INNER JOIN notebook ON note.notebook=notebook.id ORDER BY notebook.title"
#define LIST_NOTEBOOK_NOTES     "select note.text, note.due_date, note.done_at, (SELECT COUNT(*) FROM attachment WHERE attachment.note=note.id) FROM note INNER JOIN notebook ON note.notebook=notebook.id WHERE notebook.id=:notebook AND note.done_at IS NULL ORDER BY note.id"
#define LIST_ALL_NOTEBOOK_NOTES "select note.text, note.due_date, note.done_at, (SELECT COUNT(*) FROM attachment WHERE attachment.note=note.id) FROM note INNER JOIN notebook ON note.notebook=notebook.id WHERE notebook.id=:notebook ORDER BY note.id"
#define CURRENT_NOTEBOOK        "SELECT id, title FROM notebook WHERE last_used = 1"
#define SELECT_NOTEBOOK_BY_TITLE "SELECT id, title FROM notebook WHERE title=:title"
#define RESET_LAST_USED         "UPDATE notebook SET last_used=0"
#define UPDATE_LAST_USED        "UPDATE notebook SET last_used=1 WHERE id=%1"
#define INSERT_NOTEBOOK         "insert into notebook values(NULL, :title, 1);"
#define LIST_NOTEBOOKS          "SELECT title, last_used FROM notebook ORDER BY last_used, title"

// attachments
#define INSERT_ATTACHMENT       "INSERT INTO attachment(note, path, original_path, created_at) VALUES(:note, :path, :originalPath, :createdAt)"
#define LIST_NOTE_ATTACHMENTS   "SELECT id, path, original_path FROM attachment WHERE note=:note ORDER BY id"
#define DELETE_ATTACHMENT       "DELETE FROM attachment WHERE id=:id"

// tags
#define INSERT_NOTE_TAG         "INSERT OR IGNORE INTO note_tag(note, tag) VALUES(:note, :tag)"
#define DELETE_NOTE_TAG         "DELETE FROM note_tag WHERE note=:note AND tag=:tag COLLATE NOCASE"
#define LIST_NOTE_TAGS          "SELECT tag FROM note_tag WHERE note=:note ORDER BY tag"
#define LIST_ALL_TAGS           "SELECT tag, COUNT(*) FROM note_tag GROUP BY tag ORDER BY tag COLLATE NOCASE"
