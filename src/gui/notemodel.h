#pragma once

#include <QAbstractListModel>
#include <QDate>
#include <QDateTime>
#include <QSqlDatabase>
#include <QString>
#include <QStringList>
#include <QVector>

struct NoteRow {
    int id;
    QString text;
    QDate dueDate;
    QDateTime doneAt;
    int attachments;
    int notebookId;
    QString notebookTitle;
    QStringList tags;
};

class NoteModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int notebookId READ notebookId WRITE setNotebookId NOTIFY notebookIdChanged)
    Q_PROPERTY(bool includeClosed READ includeClosed WRITE setIncludeClosed NOTIFY includeClosedChanged)
    Q_PROPERTY(QString searchText READ searchText WRITE setSearchText NOTIFY searchTextChanged)
public:
    enum Roles {
        IdRole = Qt::UserRole + 1,
        TextRole,
        DueDateRole,
        DoneAtRole,
        IsDoneRole,
        IsImportantRole,
        AttachmentCountRole,
        NotebookIdRole,
        NotebookTitleRole,
        TagsRole,
    };

    explicit NoteModel(QSqlDatabase db, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    int notebookId() const { return m_notebookId; }
    void setNotebookId(int id);
    bool includeClosed() const { return m_includeClosed; }
    void setIncludeClosed(bool v);
    QString searchText() const { return m_searchText; }
    void setSearchText(const QString &text);

    Q_INVOKABLE void refresh();
    Q_INVOKABLE int rowForId(int noteId) const;

signals:
    void notebookIdChanged();
    void includeClosedChanged();
    void searchTextChanged();

private:
    QSqlDatabase m_db;
    QVector<NoteRow> m_rows;
    int m_notebookId = -1;
    bool m_includeClosed = false;
    QString m_searchText;

    void loadTags(NoteRow &row) const;
};
