#include "notebookmodel.h"

#include <QSqlQuery>

NotebookModel::NotebookModel(QSqlDatabase db, QObject *parent)
    : QAbstractListModel(parent), m_db(db)
{
    refresh();
}

int NotebookModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant NotebookModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const auto &row = m_rows.at(index.row());
    switch (role) {
    case IdRole: return row.id;
    case TitleRole: return row.title;
    case CurrentRole: return row.current;
    }
    return {};
}

QHash<int, QByteArray> NotebookModel::roleNames() const
{
    return {{IdRole, "notebookId"}, {TitleRole, "title"}, {CurrentRole, "isCurrent"}};
}

void NotebookModel::refresh()
{
    beginResetModel();
    m_rows.clear();
    QSqlQuery q(m_db);
    q.exec("SELECT id, title, last_used FROM notebook ORDER BY title COLLATE NOCASE");
    while (q.next()) {
        m_rows.push_back({q.value(0).toInt(), q.value(1).toString(), q.value(2).toBool()});
    }
    endResetModel();
}

int NotebookModel::currentNotebookId() const
{
    for (const auto &r : m_rows) if (r.current) return r.id;
    return m_rows.isEmpty() ? -1 : m_rows.first().id;
}

QString NotebookModel::currentNotebookTitle() const
{
    for (const auto &r : m_rows) if (r.current) return r.title;
    return m_rows.isEmpty() ? QString() : m_rows.first().title;
}
