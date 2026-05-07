#pragma once

#include <QJsonArray>
#include <QSqlQuery>
#include <QString>
#include <QStringList>

QString tagFilterClause(int n);
void bindTags(QSqlQuery &query, const QStringList &tags);

void printNoteTable(QSqlQuery &query, const QString &emptyMessage);
QJsonArray collectNotesJson(QSqlQuery &query);
void printNotesJson(QSqlQuery &query);
