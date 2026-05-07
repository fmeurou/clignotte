#pragma once

#include <QChar>
#include <QMap>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QStringList>

void list(QSqlQuery &query, QSqlDatabase db, bool jsonOut, bool includeClosed, const QStringList &tags, const QString &notebook);
void agenda(QSqlQuery &query, QSqlDatabase db, int days, const QString &emptyMessage, bool jsonOut, bool includeClosed, const QStringList &tags, const QString &notebook);
void search(QSqlQuery &query, QSqlDatabase db, const QString &searchTerm, bool jsonOut, bool includeClosed, const QStringList &tags, const QString &notebook);
void pretty(QSqlQuery &query, QSqlDatabase db, bool includeClosed);
void byTag(QSqlQuery &query, QSqlDatabase db, bool jsonOut, bool includeClosed, const QString &notebook);

void addNote(QSqlQuery &query, QSqlDatabase db, QString text, QMap<QString, QString> currentNotebook);
void editNote(QSqlQuery &query, QSqlDatabase db, const QString &noteIdStr);
void moveNote(QSqlQuery &query, QSqlDatabase db, const QString &noteIdStr, const QString &notebookTitle);
void deleteNote(QSqlQuery &query, QSqlDatabase db, QString note);
void endNote(QSqlQuery &query, QSqlDatabase db, QString note);
void markNote(QSqlQuery &query, QSqlDatabase db, QString note, QChar prefix);
void setDueDate(QSqlQuery &query, QSqlDatabase db, QString note, QString dueDateString);
