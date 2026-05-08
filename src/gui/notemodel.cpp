#include "notemodel.h"

#include <QSqlQuery>
#include <QVariant>

NoteModel::NoteModel(QSqlDatabase db, QObject *parent)
    : QAbstractListModel(parent), m_db(db)
{
}

int NoteModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant NoteModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_rows.size())
        return {};
    const auto &row = m_rows.at(index.row());
    switch (role) {
    case IdRole: return row.id;
    case TextRole: return row.text;
    case DueDateRole: return row.dueDate.isValid() ? QVariant(row.dueDate) : QVariant();
    case DoneAtRole: return row.doneAt.isValid() ? QVariant(row.doneAt) : QVariant();
    case IsDoneRole: return row.doneAt.isValid();
    case IsImportantRole: return row.text.startsWith('!');
    case AttachmentCountRole: return row.attachments;
    case NotebookIdRole: return row.notebookId;
    case NotebookTitleRole: return row.notebookTitle;
    case TagsRole: return row.tags;
    }
    return {};
}

QHash<int, QByteArray> NoteModel::roleNames() const
{
    return {
        {IdRole, "noteId"},
        {TextRole, "text"},
        {DueDateRole, "dueDate"},
        {DoneAtRole, "doneAt"},
        {IsDoneRole, "isDone"},
        {IsImportantRole, "isImportant"},
        {AttachmentCountRole, "attachmentCount"},
        {NotebookIdRole, "notebookId"},
        {NotebookTitleRole, "notebookTitle"},
        {TagsRole, "tags"},
    };
}

void NoteModel::setNotebookId(int id)
{
    if (m_notebookId == id) return;
    m_notebookId = id;
    emit notebookIdChanged();
    refresh();
}

void NoteModel::setIncludeClosed(bool v)
{
    if (m_includeClosed == v) return;
    m_includeClosed = v;
    emit includeClosedChanged();
    refresh();
}

void NoteModel::setSearchText(const QString &text)
{
    if (m_searchText == text) return;
    m_searchText = text;
    emit searchTextChanged();
    refresh();
}

void NoteModel::loadTags(NoteRow &row) const
{
    QSqlQuery q(m_db);
    q.prepare("SELECT tag FROM note_tag WHERE note=:note ORDER BY tag");
    q.bindValue(":note", row.id);
    if (q.exec()) {
        while (q.next()) row.tags << q.value(0).toString();
    }
}

void NoteModel::refresh()
{
    beginResetModel();
    m_rows.clear();
    QString sql =
        "SELECT note.id, note.text, note.due_date, note.done_at, "
        "(SELECT COUNT(*) FROM attachment WHERE attachment.note=note.id), "
        "notebook.id, notebook.title "
        "FROM note INNER JOIN notebook ON note.notebook=notebook.id ";
    QStringList where;
    if (m_notebookId >= 0) where << "notebook.id=:nb";
    if (!m_includeClosed) where << "note.done_at IS NULL";
    if (!m_searchText.trimmed().isEmpty()) where << "note.text LIKE :search";
    if (!where.isEmpty()) sql += "WHERE " + where.join(" AND ") + " ";
    sql += "ORDER BY note.due_date DESC, note.id DESC";

    QSqlQuery q(m_db);
    q.prepare(sql);
    if (m_notebookId >= 0) q.bindValue(":nb", m_notebookId);
    if (!m_searchText.trimmed().isEmpty()) q.bindValue(":search", "%" + m_searchText.trimmed() + "%");
    if (q.exec()) {
        while (q.next()) {
            NoteRow row;
            row.id = q.value(0).toInt();
            row.text = q.value(1).toString();
            row.dueDate = q.value(2).toDate();
            row.doneAt = q.value(3).toDateTime();
            row.attachments = q.value(4).toInt();
            row.notebookId = q.value(5).toInt();
            row.notebookTitle = q.value(6).toString();
            loadTags(row);
            m_rows.push_back(row);
        }
    }
    endResetModel();
}

int NoteModel::rowForId(int noteId) const
{
    for (int i = 0; i < m_rows.size(); ++i)
        if (m_rows.at(i).id == noteId) return i;
    return -1;
}
