#pragma once

#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>

void exportNotes(QSqlQuery &query, QSqlDatabase db, const QString &dirPath);
void syncNotes(QSqlDatabase db, const QString &dirPath, bool deleteLocal);
