#include "clignotte.h"

//Clignotte
Clignotte::Clignotte(QString location)  {
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    QDir storage(location);
    m_db.setDatabaseName(storage.absoluteFilePath("notes.db"));
}

bool Clignotte::initDb()   {
    if(!m_db.open()) {
        qCritical() << "unable to open db";
        qDebug() << m_db.lastError().text();
        return false;
    }   else    {
        QSqlQuery query = QSqlQuery(m_db);
        if(!m_db.tables().contains("note"))  {
            qInfo() << "table note is not present, creating";
            if(!(query.exec(QString(CREATE_NOTEBOOK_TABLE))
                    &&
                 query.exec(QString(CREATE_NOTE_TABLE))
                 ))
            {
                qCritical()  << "error creating database";
                qDebug() << query.lastQuery() << query.lastError() << m_db.lastError();
                exit(-1);
            }   else    {
                qInfo()  << "inserting default notebook";
                query.exec(QString(INIT_NOTEBOOK_TABLE));

            }
        }
        return true;
    }
}

QList<Note> Clignotte::notes() {
    QList<Note> notes;
    QSqlQuery query = QSqlQuery(m_db);
    QString listQuery = QString(LIST_NOTES);
    if(query.exec(listQuery))    {
        if(query.first())   {
            QSqlRecord record;
            Note note(m_db);
            do  {
                record = query.record();
                note.setNotebookId(record.value(0).toInt());
                note.setId(record.value(1).toInt());
                note.setCreatedAt(record.value(2).toDateTime());
                note.setDueDate(record.value(3).toDate());
                note.setDoneDate(record.value(4).toDateTime());
                note.setText(record.value(5).toString());
                notes.append(note);
            }   while(query.next());
        }
    }
    return notes;
}

QList<Notebook> Clignotte::notebooks() {
    QList<Notebook> notebooks;
    QSqlQuery query = QSqlQuery(m_db);
    QString listQuery = QString(LIST_NOTEBOOKS);
    if(query.exec(listQuery))    {
        if(query.first())   {
            QSqlRecord record;
            Notebook notebook(m_db);
            do  {
                record = query.record();
                notebook.setId(record.value(0).toInt());
                notebook.setTitle(record.value(1).toString());
                notebooks.append(notebook);
            }   while(query.next());
        }
    }
    return notebooks;
}

Notebook Clignotte::currentNotebook()    {
    Notebook currentNotebook(m_db);
    QSqlQuery query = QSqlQuery(m_db);
    if(query.exec(CURRENT_NOTEBOOK))    {
        if(query.first())   {
            QSqlRecord record;
            record = query.record();
            currentNotebook.setId(record.value(0).toInt());
            currentNotebook.setTitle(record.value(1).toString());
        }
    } else    {
        qCritical()  << "error querying database\n";
        qDebug() << query.lastQuery() << query.lastError() << m_db.lastError();
    }
    return currentNotebook;
}

//Notebook
Notebook::Notebook(QSqlDatabase v_db) {
    m_db = v_db;
}

Notebook Notebook::get(QSqlDatabase v_db, int v_id)    {
    Notebook notebook(v_db);
    QSqlQuery query = QSqlQuery(v_db);
    query.prepare(QString(GET_NOTEBOOK_BY_ID));
    query.bindValue(":id", v_id);
    if(query.exec())    {
        if(query.first())   {
            QSqlRecord record;
            record = query.record();
            notebook.setId(record.value(0).toInt());
            notebook.setTitle(record.value(1).toString());
        }   else    {
            qDebug() << "no such notebook";
        }
    } else  {
        qCritical()  << "error querying database\n";
        qDebug() << query.lastQuery() << query.lastError() << v_db.lastError();
    }
}

int    Notebook::save()    {
    QSqlQuery query = QSqlQuery(m_db);
    query.prepare(GET_NOTEBOOK_BY_TITLE);
    query.bindValue(":title", m_title);
    query.exec();
    if(query.first())   {
        QSqlRecord record;
        record = query.record();
        query.exec(RESET_LAST_USED);
        query.exec(QString(UPDATE_LAST_USED).arg(record.value(0).toString()));
        return 1;
    } else    {
        query.exec(RESET_LAST_USED);
        query.prepare(INSERT_NOTEBOOK);
        query.bindValue(":title", m_title);
        if(query.exec())  {
            return 2;
        } else    {
            qCritical()  << "error saving notebook to database\n";
            qDebug() << query.lastQuery() << query.lastError() << m_db.lastError();
        }
    }
    return 0;
}

int     Notebook::getId()   {return m_id;}
QString Notebook::getTitle()    {return m_title;}
void     Notebook::setId(int v_id)   {m_id = v_id;}
void Notebook::setTitle(QString v_title)    {m_title = v_title;}

int     Notebook::addNote(QString text)    {
    Note note(m_db);
    note.setText(text);
    note.setNotebookId(m_id);
    note.save();
}

QList<Note> Notebook::notes() {
    QList<Note> notes;
    QSqlQuery query = QSqlQuery(m_db);
    query.prepare(QString(LIST_NOTEBOOK_NOTES));
    query.bindValue(":notebook", m_id);
    if(query.exec())    {
        if(query.first())   {
            QSqlRecord record;
            Note note(m_db);
            do  {
                record = query.record();
                note.setNotebookId(record.value(0).toInt());
                note.setId(record.value(1).toInt());
                note.setCreatedAt(record.value(2).toDateTime());
                note.setDueDate(record.value(3).toDate());
                note.setDoneDate(record.value(4).toDateTime());
                note.setText(record.value(5).toString());
                notes.append(note);
            }   while(query.next());
        }
    }   else    {
        qCritical()  << "error querying database\n";
        qDebug() << query.lastQuery() << query.lastError() << m_db.lastError();
    }
    return notes;
}


//Note
Note::Note(QSqlDatabase v_db) {
    m_db = v_db;
}

Note Note::get(int id)  {

}

bool Note::isImportant()    {
    return m_text.startsWith("!");
}

bool Note::isBold() {
    return m_text.startsWith("*");
}

bool Note::isDue() {
    return m_dueDate < QDate::currentDate();
}

bool Note::isDone() {
    return m_doneDate.isValid();
}

bool Note::save()    {
    QSqlQuery query = QSqlQuery(m_db);
    query.prepare(QString(INSERT_NOTE));
    query.bindValue(":currentDateTime", QDateTime::currentDateTime());
    query.bindValue(":notebook", m_notebookId);
    query.bindValue(":text", m_text);
    if(query.exec())  {
        qInfo()  << "note added";
        return true;
    } else    {
        qCritical() << "error saving note to database";
        qDebug() << query.lastQuery() << query.lastError() << m_db.lastError();
        return false;
    }
}

bool Note::updateDueDate(QDate dueDate)    {
    QSqlQuery query = QSqlQuery(m_db);
    if(m_id)    {
        query.prepare(QString(UPDATE_DUE_DATE));
        query.bindValue(":dueDate", dueDate);
        query.bindValue(":id", m_id);
        if(query.exec())  {
            qInfo()  << "note updated\n";
            return true;
        } else    {
            qCritical() << "error updating note\n";
            qDebug() << query.lastQuery() << query.lastError() << query.boundValues() << m_db.lastError();
            return false;
        }
    }    else    {
        qWarning() << "invalid note identifier\n";
        return false;
    }
}

bool Note::setDone()    {
    QSqlQuery query = QSqlQuery(m_db);
    if(m_id)    {
        query.prepare(QString(UPDATE_DONE));
        query.bindValue(":currentDateTime", QDateTime::currentDateTime());
        query.bindValue(":id", m_id);
        if(query.exec())  {
            qInfo()  << "note closed";
            return true;
        } else    {
            qCritical() << "error closing note";
            qDebug() << query.lastQuery() << query.lastError() << query.boundValues() << m_db.lastError();
            return false;
        }
    }    else    {
        qWarning() << "invalid note identifier";
        return false;
    }
}

bool Note::remove() {
    QSqlQuery query = QSqlQuery(m_db);
    if(m_id)    {
        query.prepare(QString(DELETE_NOTE));
        query.bindValue(":id", m_id);
        if(query.exec())  {
            qInfo()  << "note deleted";
        } else    {
            qCritical()  << "error deleting note from database";
            qDebug() << query.lastQuery() << query.lastError() << m_db.lastError();
        }
    }    else    {
        qWarning() << "invalid note identifier\n";
    }
}
