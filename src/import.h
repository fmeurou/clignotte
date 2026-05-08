#pragma once

#include <QMap>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QString>

void importNotes(QSqlQuery &query, QSqlDatabase db, const QString &path, const QMap<QString, QString> &currentNotebook);
