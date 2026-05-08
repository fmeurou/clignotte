#pragma once

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>
#include <QStringList>

void tagNote(QSqlQuery &query, QSqlDatabase db, const QString &noteIdStr, const QStringList &tags);
void untagNote(QSqlQuery &query, QSqlDatabase db, const QString &noteIdStr, const QStringList &tags);
void listAllTags(QSqlQuery &query, QSqlDatabase db);
void listNoteTags(QSqlQuery &query, QSqlDatabase db, const QString &noteIdStr);
