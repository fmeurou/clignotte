#pragma once

#include <QMap>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>

QMap<QString, QString> getCurrentNotebook(QSqlQuery &query, QSqlDatabase db);
void setNotebookByTitle(QSqlQuery &query, QSqlDatabase db, QString title);
void listNotebooks(QSqlQuery &query, QSqlDatabase db);
void closeNotebook(QSqlQuery &query, QSqlDatabase db, const QString &title);
