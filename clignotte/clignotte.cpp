#include "clignotte.h"

//Clignotte
Clignotte::Clignotte(QString location) {
    m_location = location;
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    QDir storage(location);
    m_db.setDatabaseName(storage.absoluteFilePath("notes.db"));
}

bool Clignotte::clearDb()   {
    QFile db(m_location);
    return db.remove();
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
        query.clear();
        return true;
    }
}

QList<Note> Clignotte::notes() {
    QList<Note> notes;
    QSqlQuery query(m_db);
    QString listQuery = QString(LIST_NOTES);
    if(query.exec(listQuery))    {
        if(query.first())   {
            do  {
                Note note = Note::create(m_db, query.record());
                notes.append(note);
            }   while(query.next());
        }
    }
    query.clear();
    return notes;
}

QList<Notebook> Clignotte::notebooks(const int filter) {
    QList<Notebook> notebooks;
    QSqlQuery query(m_db);
    QString listQuery = QString(LIST_NOTEBOOKS);
    if(filter == Clignotte::Active)  {
        QString listQuery = QString(LIST_ACTIVE_NOTEBOOKS);
    }   else if(filter == Clignotte::Current)    {
        QString listQuery = QString(CURRENT_NOTEBOOK);
    }
    if(query.exec(listQuery))    {
        if(query.first())   {
            do  {
                Notebook notebook = Notebook::create(m_db, query.record());
                notebooks.append(notebook);
            }   while(query.next());
        }
    }
    query.clear();
    return notebooks;
}

Notebook Clignotte::currentNotebook()    {
    Notebook currentNotebook(m_db);
    QSqlQuery query(m_db);
    if(query.exec(CURRENT_NOTEBOOK))    {
        if(query.first())   {
            currentNotebook = Notebook::create(m_db, query.record());
        }
    } else    {
        qCritical()  << "error querying database\n";
        qDebug() << query.lastQuery() << query.lastError() << m_db.lastError();
    }
    query.clear();
    return currentNotebook;
}

Notebook Clignotte::getNotebook(int v_id)    {
    return Notebook::get(m_db, v_id);
}

Notebook Clignotte::getNotebook(QString v_title)    {
    return Notebook::get(m_db, v_title);
}

Note Clignotte::getNote(int v_id)    {
    return Note::get(m_db, v_id);
}

int Clignotte::createNotebook(QString v_title) {
    Notebook notebook(m_db);
    notebook.setTitle(v_title);
    return notebook.save();
}

int Clignotte::createNote(QString v_text)   {
    Notebook notebook = currentNotebook();
    return notebook.addNote(v_text);
}

//Notebook
Notebook::Notebook(QSqlDatabase v_db) {
    m_db = v_db;
}

Notebook Notebook::create(QSqlDatabase v_db, QSqlRecord record)    {
    Notebook notebook(v_db);
    notebook.setId(record.value(0).toInt());
    notebook.setTitle(record.value(1).toString());
    notebook.setLastUsed(record.value(2).toBool());
    return notebook;
}

Notebook Notebook::get(QSqlDatabase v_db, int v_id)    {
    QSqlQuery query(v_db);
    query.prepare(QString(GET_NOTEBOOK_BY_ID));
    query.bindValue(":id", v_id);
    if(query.exec())    {
        if(query.first())   {
            Notebook notebook = Notebook::create(v_db, query.record());
            query.clear();
            return notebook;
        }   else    {
            qDebug() << "no such notebook";
        }
    } else  {
        qCritical()  << "error querying database\n";
        qDebug() << query.lastQuery() << query.lastError() << v_db.lastError();
    }
    query.clear();
    return Notebook(v_db);
}

Notebook Notebook::get(QSqlDatabase v_db, QString v_title)    {
    // Finds Notebook by title and sets it at last used notebook
    Notebook notebook(v_db);
    notebook.setTitle(v_title);
    notebook.save();
    return notebook;
}

int    Notebook::save()    {
    // Creates a new notebook in database or set the existing one as last used based on its title
    QSqlQuery query = QSqlQuery(m_db);
    query.prepare(GET_NOTEBOOK_BY_TITLE);
    query.bindValue(":title", m_title);
    query.exec();
    if(query.first())   {
        QSqlRecord record = query.record();
        query.exec(RESET_LAST_USED);
        query.exec(QString(UPDATE_LAST_USED).arg(record.value(0).toString()));
        query.clear();
        return 1;
    } else    {
        query.exec(RESET_LAST_USED);
        query.prepare(INSERT_NOTEBOOK);
        query.bindValue(":title", m_title);
        if(query.exec())  {
            query.clear();
            return 2;
        } else    {
            qCritical()  << "error saving notebook to database\n";
            qDebug() << query.lastQuery() << query.lastError() << m_db.lastError();
        }
    }
    query.clear();
    return 0;
}

int     Notebook::getId()   {return m_id;}
QString Notebook::getTitle()    {return m_title;}
bool    Notebook::getLastUsed() {return m_lastUsed;}
void     Notebook::setId(int v_id)   {m_id = v_id;}
void Notebook::setTitle(QString v_title)    {m_title = v_title;}
void Notebook::setLastUsed(bool v_lastUsed) {m_lastUsed = v_lastUsed;}


int     Notebook::addNote(QString text)    {
    Note note(m_db);
    note.setText(text);
    note.setNotebookId(m_id);
    return note.save();
}

QList<Note> Notebook::notes() {
    QList<Note> notes;
    QSqlQuery query(m_db);
    query.prepare(QString(LIST_NOTEBOOK_NOTES));
    query.bindValue(":notebook", m_id);
    if(query.exec())    {
        if(query.first())   {
            do  {
                Note note = Note::create(m_db, query.record());
                notes.append(note);
            }   while(query.next());
        }
    }   else    {
        qCritical()  << "error querying database\n";
        qDebug() << query.lastQuery() << query.lastError() << m_db.lastError();
    }
    query.clear();
    return notes;
}


//Note
Note::Note(QSqlDatabase v_db) {
    m_db = v_db;
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

void Note::encrypt(QString email)   {
    QUuid uuid = QUuid::createUuid();
    QString fileName = QString("/tmp/%1").arg(uuid.toString());
    QFile tmpFile(fileName);
    if(tmpFile.open(QFile::ReadWrite))  {
        tmpFile.write(m_text.toLatin1());
    }
    tmpFile.close();
    QProcess process;
    QString command;
    if(email.isEmpty()) {
        command = QString("gpg --output %1 --encrypt %1").arg(fileName);
    }   else    {
        command = QString("gpg --output %1 --encrypt --recipient %2 %1").arg(fileName).arg(email);
    }
    qDebug() << command;
    process.start(command);
    while(process.waitForFinished()) {};
    tmpFile.open(QFile::ReadOnly);
    QString output = tmpFile.readAll();
    tmpFile.close();
    tmpFile.remove();
    updateText(output);
}

QString Note::decrypt()   {
    QUuid uuid = QUuid::createUuid();
    QString fileName = QString("/tmp/%1").arg(uuid.toString());
    QFile tmpFile(fileName);
    if(tmpFile.open(QFile::ReadWrite))  {
        tmpFile.write(m_text.toLatin1());
    }
    tmpFile.close();
    QProcess process;
    QString command;
    command = QString("gpg --output %1 --decrypt %1").arg(fileName);
    process.start(command);
    while(process.waitForFinished()) {};
    tmpFile.open(QFile::ReadOnly);
    QString output = tmpFile.readAll();
    tmpFile.close();
    tmpFile.remove();
    return output;
}

Note Note::get(QSqlDatabase v_db, int v_id)    {
    QSqlQuery query = QSqlQuery(v_db);
    query.prepare(QString(GET_NOTE_BY_ID));
    query.bindValue(":id", v_id);
    if(query.exec())    {
        if(query.first())   {
            Note note = Note::create(v_db, query.record());
            query.clear();
            return note;
        }   else    {
            qDebug() << "no such note";
        }
    } else  {
        qCritical()  << "error querying database\n";
        qDebug() << query.lastQuery() << query.lastError() << v_db.lastError();
    }
    query.clear();
    return Note(v_db);
}

Notebook Note::getNotebook()    {
    return Notebook::get(m_db, m_notebookId);
}

Note Note::create(QSqlDatabase v_db, QSqlRecord record) {
    Note note(v_db);
    note.setNotebookId(record.value(0).toInt());
    note.setId(record.value(1).toInt());
    note.setCreatedAt(record.value(2).toDateTime());
    note.setDueDate(record.value(3).toDate());
    note.setDoneDate(record.value(4).toDateTime());
    note.setText(record.value(5).toString());
    return note;
}

bool Note::save()    {
    QSqlQuery query = QSqlQuery(m_db);
    query.prepare(QString(INSERT_NOTE));
    query.bindValue(":currentDateTime", QDateTime::currentDateTime());
    query.bindValue(":notebook", m_notebookId);
    query.bindValue(":text", m_text);
    if(query.exec())  {
        qInfo()  << "note added";
        query.clear();
        return true;
    } else    {
        qCritical() << "error saving note to database";
        qDebug() << query.lastQuery() << query.lastError() << m_db.lastError();
        return false;
    }
    query.clear();
    return false;
}

bool Note::updateDueDate(QDate dueDate)    {
    QSqlQuery query = QSqlQuery(m_db);
    if(m_id)    {
        query.prepare(QString(UPDATE_DUE_DATE));
        query.bindValue(":dueDate", dueDate);
        query.bindValue(":id", m_id);
        if(query.exec())  {
            qInfo()  << "note updated\n";
            query.clear();
            return true;
        } else    {
            qCritical() << "error updating note\n";
            qDebug() << query.lastQuery() << query.lastError() << query.boundValues() << m_db.lastError();
        }
    }    else    {
        qWarning() << "invalid note identifier\n";
    }
    query.clear();
    return false;
}

bool Note::updateText(QString text)    {
    QSqlQuery query = QSqlQuery(m_db);
    if(m_id)    {
        query.prepare(QString(UPDATE_TEXT));
        query.bindValue(":text", text);
        query.bindValue(":id", m_id);
        if(query.exec())  {
            qInfo()  << "note updated\n";
            query.clear();
            return true;
        } else    {
            qCritical() << "error updating note\n";
            qDebug() << query.lastQuery() << query.lastError() << query.boundValues() << m_db.lastError();
        }
    }    else    {
        qWarning() << "invalid note identifier\n";
    }
    query.clear();
    return false;
}

bool Note::setDone()    {
    QSqlQuery query = QSqlQuery(m_db);
    if(m_id)    {
        query.prepare(QString(UPDATE_DONE));
        query.bindValue(":currentDateTime", QDateTime::currentDateTime());
        query.bindValue(":id", m_id);
        if(query.exec())  {
            qInfo()  << "note closed";
            query.clear();
            return true;
        } else    {
            qCritical() << "error closing note";
            qDebug() << query.lastQuery() << query.lastError() << query.boundValues() << m_db.lastError();
        }
    }    else    {
        qWarning() << "invalid note identifier";
    }
    query.clear();
    return false;
}

bool Note::remove() {
    QSqlQuery query = QSqlQuery(m_db);
    if(m_id)    {
        query.prepare(QString(DELETE_NOTE));
        query.bindValue(":id", m_id);
        if(query.exec())  {
            qInfo()  << "note deleted";
            query.clear();
            return 1;
        } else    {
            qCritical()  << "error deleting note from database";
            qDebug() << query.lastQuery() << query.lastError() << m_db.lastError();
        }
    }    else    {
        qWarning() << "invalid note identifier\n";
    }
    query.clear();
    return 0;
}
