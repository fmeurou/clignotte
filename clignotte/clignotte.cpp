#include "clignotte.h"
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QDebug>
#include <QTextStream>
#include <QDateTime>

Clignotte::Clignotte()
{}



void list(QSqlQuery query, QSqlDatabase db) {
    QString listQuery;
    out.setFieldAlignment(QTextStream::AlignCenter);
    listQuery = QString(LIST_NOTES);
    if(query.exec(listQuery))    {
        if(query.first())   {
            QSqlRecord record;
            out     << UNDERLINED_TEXT<< "|"
                    << qSetFieldWidth(20) << "notebook"
                    << qSetFieldWidth(1) << "|"
                    << qSetFieldWidth(4) << "id"
                    << qSetFieldWidth(1) << "|"
                    << qSetFieldWidth(20) << "due date"
                    << qSetFieldWidth(1) << "|"
                    << qSetFieldWidth(4) << "done"
                    << qSetFieldWidth(1) << "|"
                    << qSetFieldWidth(81) << "note"
                    << qSetFieldWidth(1) << "|\n"
                    << NORMAL_TEXT;
            out.setFieldAlignment(QTextStream::AlignLeft);
            do  {
                record = query.record();
                QString text = record.value(4).toString();
                bool isImportant = text.startsWith("!");
                bool isBold = text.startsWith("*");
                bool isDue = record.value(2).toDate().isValid() && record.value(2).toDate().operator <(QDate::currentDate());
                bool isDone = record.value(3).toBool();
                if(isDone)   {
                    out << ITALIC_TEXT;
                }   else if(isDue)   {
                    out << URGENT_TEXT;
                }   else if(isImportant)  {
                    out << IMPORTANT_TEXT;
                }     else if(isBold) {
                    out << INVERTED_TEXT;
                }   else    {
                    out << NORMAL_TEXT;
                }
                out     << qSetFieldWidth(1) << "|"
                        << qSetFieldWidth(20) << record.value(0).toString().left(20)
                        << qSetFieldWidth(1) << "|"
                        << qSetFieldWidth(4) << record.value(1).toString()
                        << qSetFieldWidth(1) << "|"
                        << qSetFieldWidth(20) << record.value(2).toDate().toString(Qt::ISODate).left(20)
                        << qSetFieldWidth(1) << "|"
                        << qSetFieldWidth(4) << record.value(3).toBool()
                        << qSetFieldWidth(1) << "|"
                        << qSetFieldWidth(81) << record.value(4).toString().left(31)
                        << qSetFieldWidth(1) << "|";
                out << NORMAL_TEXT;
                out << "\n";
            }   while(query.next());
            out.flush();
        }   else    {
            qInfo()  << QCoreApplication::translate("main", "No note available\n");
        }
    } else    {
        qCritical()  << "error querying database\n";
        qDebug() << query.lastQuery() << query.lastError() << db.lastError();
    }
    exit(0);
}

void pretty(QSqlQuery query, QSqlDatabase db)   {
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
                            bool isDue = dueDate.isValid() && (dueDate < QDate::currentDate());
                            bool isDone = subrecord.value(2).toBool();
                            out  << "\t";
                            if(isDone)   {
                                out << ITALIC_TEXT;
                            }   else if(isDue)   {
                                out << URGENT_TEXT;
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

void addNote(QSqlQuery query, QSqlDatabase db, QString text, QMap<QString, QString> currentNotebook)    {
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

void setDueDate(QSqlQuery query, QSqlDatabase db, QString note, QString dueDateString)    {
    int noteId = QVariant(note).toInt();
    if(noteId)    {
        QDate dueDate = QDate::fromString(dueDateString, "yyyy-MM-dd");
        if(!dueDate.isValid())  {
            qWarning() << "invalid date format, use yyyy-MM-dd (2016-12-31)";
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

void endNote(QSqlQuery query, QSqlDatabase db, QString note)    {
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

void deleteNote(QSqlQuery query, QSqlDatabase db, QString note) {
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

QMap<QString, QString> getCurrentNotebook(QSqlQuery query, QSqlDatabase db)    {
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

void setNotebookByTitle(QSqlQuery query, QSqlDatabase db, QString title)    {
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

void listNotebooks(QSqlQuery query, QSqlDatabase db)    {
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
}
