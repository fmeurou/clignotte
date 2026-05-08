#pragma once

#include <QAbstractListModel>
#include <QSqlDatabase>
#include <QString>
#include <QVector>

struct NotebookRow {
    int id;
    QString title;
    bool current;
};

class NotebookModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum Roles { IdRole = Qt::UserRole + 1, TitleRole, CurrentRole };

    explicit NotebookModel(QSqlDatabase db, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE void refresh();
    Q_INVOKABLE int currentNotebookId() const;
    Q_INVOKABLE QString currentNotebookTitle() const;

private:
    QSqlDatabase m_db;
    QVector<NotebookRow> m_rows;
};
