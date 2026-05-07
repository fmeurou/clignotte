#pragma once

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>

void attachFile(QSqlQuery &query, QSqlDatabase db, const QString &noteIdStr, const QString &path, bool copy, const QString &storageDir);
void listAttachments(QSqlQuery &query, QSqlDatabase db, const QString &noteIdStr);
void detachFile(QSqlQuery &query, QSqlDatabase db, const QString &attachIdStr);
