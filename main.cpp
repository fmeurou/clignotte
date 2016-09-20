#include <QCoreApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QStandardPaths>
#include <QDir>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QSqlError>
#include <QDebug>
#include <QTextStream>
#include <QDateTime>
#include <iostream>


int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QString notebookTable = "notebook";
    QString noteTable = "note";
    QString currentNotebookId = 0;
    QString currentNotebookName = "";
    QCoreApplication::setApplicationName("clignotte");
    QCoreApplication::setApplicationVersion("1.0");

    QString storedNotes = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if(!QDir(storedNotes).exists() && !QDir(storedNotes).mkpath(storedNotes)) {
        std::cout  << "unable to create directory, exiting...";
        exit(-1);
    }
    QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
    QSqlQuery query;
    QDir storage(storedNotes);
    db.setDatabaseName(storage.absoluteFilePath("notes.db"));
    if(!db.open()) {
        qDebug() << "unable to open db";
        std::cout  << db.lastError().text().toStdString();
        exit(-2);
    }   else    {
        query = QSqlQuery(db);
        if(!db.tables().contains(noteTable))  {
            qDebug() << "table note is not present, creating";
            if(!(query.exec(QString("CREATE TABLE notebook(id INTEGER PRIMARY KEY, title TEXT, last_used BOOL);"))
                    &&
                 query.exec(QString("CREATE TABLE note(id INTEGER PRIMARY KEY, created_at DATETIME, text TEXT, notebook INTEGER, FOREIGN KEY(notebook) REFERENCES notebook(id));"))
                 ))
            {
                qDebug() << query.lastQuery() << query.lastError() << db.lastError();
                std::cout  << "error creating database";
                exit(-1);
            }   else    {
                std::cout  << "inserting default notebook";
                query.exec(QString("INSERT INTO notebook(id, title, last_used) values(0, 'default', 1);"));

            }
        }   else    {
            query.exec("SELECT id, title FROM notebook WHERE last_used = 1");
            if(query.first())   {
                QSqlRecord record;
                record = query.record();
                currentNotebookId = record.value(0).toString();
                currentNotebookName = record.value(1).toString();
            }

        }
    }
      QCommandLineParser parser;
      parser.setApplicationDescription("Note helper");
      parser.addHelpOption();
      parser.addVersionOption();
      parser.addPositionalArgument("command", QCoreApplication::translate("main", "what to do (list, add, delete)"));
      parser.addPositionalArgument("content", QCoreApplication::translate("main", "note id or text"));

      // Process the actual command line arguments given by the user
      parser.process(app);

      const QStringList args = parser.positionalArguments();
      if(!args.length() || args.at(0) == "list")  {
            QString listQuery;
            listQuery = "select notebook.title, note.id, note.created_at, note.text FROM note INNER JOIN notebook ON note.notebook=notebook.id";
            if(query.exec(listQuery))    {
                if(query.first())   {
                    QSqlRecord record;
                    std::cout  << "  notebook   |   id   |    creation date    |  note \n";
                    std::cout  << QString("").fill('-',80).toStdString() << "\n";
                    do  {
                        record = query.record();
                        std::cout
                                << record.value(0).toString().toStdString()
                                << " | "
                                << QString().fill(' ', 6-record.value(0).toString().length()).toStdString()
                                << record.value(1).toString().toStdString()
                                << " | "
                                << record.value(2).toDateTime().toString(Qt::ISODate).toStdString()
                                << " | "
                                << record.value(3).toString().toStdString()
                                << "\n";

                    }   while(query.next());
                }   else    {
                    std::cout  << QCoreApplication::translate("main", "No note available\n").toStdString();
                }
            } else    {
                std::cout  << "error querying database\n";
                qDebug() << query.lastQuery() << query.lastError() << db.lastError();
            }
            exit(0);
      }
      // source is args.at(0), destination is args.at(1)
      if(args.at(0) == "notebook")   {
          if(args.length() < 2) {
              std::cout  << "no notebook to choose or create...";
          }
          else {
              query.prepare("SELECT id, title FROM notebook WHERE title=:title");
              query.bindValue(":title", args.mid(1).join(" "));
              query.exec();
              if(query.first())   {
                  QSqlRecord record;
                  record = query.record();
                  currentNotebookId = record.value(0).toString();
                  currentNotebookName = record.value(1).toString();
                  qDebug() << currentNotebookId << currentNotebookName;
                  query.exec("UPDATE notebook SET last_used=0");
                  query.exec(QString("UPDATE notebook SET last_used=1 WHERE id=%1").arg(QVariant(currentNotebookId).toString()));
                  std::cout  << "notebook selected\n";
              } else    {
                  query.exec("UPDATE notebook SET last_used=0");
                  qDebug() << args.mid(1).join(" ");
                  QString text = args.mid(1).join(" ");
                  query.prepare("insert into notebook values(NULL, :title, 1);");
                  query.bindValue(":title", args.mid(1).join(" "));
                  if(query.exec())  {
                      std::cout  << "notebook added\n";
                  } else    {
                      std::cout  << "error saving notebook to database\n";
                      qDebug() << query.lastQuery() << query.lastError() << db.lastError();
                  }
                  query.exec("SELECT id, title FROM notebook WHERE last_used = 1");
                  if(query.first())   {
                      QSqlRecord record;
                      record = query.record();
                      currentNotebookId = record.value(0).toString();
                      currentNotebookName = record.value(1).toString();
                      qDebug() << currentNotebookId << currentNotebookName;
                  }
              }
          }
          exit(0);
      }

      if(args.at(0) == "notebooks")  {
            QString listQuery;
            listQuery = "SELECT title, last_used FROM notebook ORDER BY last_used, title";
            if(query.exec(listQuery))    {
                if(query.first())   {
                    QSqlRecord record;
                    std::cout  << "  title (* last_used) \n";
                    std::cout  << QString("").fill('-',80).toStdString() << "\n";
                    do  {
                        record = query.record();
                        QString lastUsed = "";
                        if(record.value(1).toBool()) lastUsed="(*)";
                        std::cout
                                << record.value(0).toString().toStdString()
                                << lastUsed.toStdString()
                                << "\n";
                    }   while(query.next());
                }   else    {
                    std::cout  << QCoreApplication::translate("main", "No note available\n").toStdString();
                }
            } else    {
                std::cout  << "error querying database\n";
                qDebug() << query.lastQuery() << query.lastError() << db.lastError();
            }
            exit(0);
      }


      if(args.at(0) == "add")   {
          if(args.length() < 2) {
              std::cout  << "no note to add...";
          }
          else {
              qDebug() << args.mid(1).join(" ") << currentNotebookId;
              QString text = args.mid(1).join(" ");
              query.prepare(QString("insert into %1(notebook, created_at, text) values(:notebook, :currentDateTime, :text)").arg(noteTable));
              query.bindValue(":currentDateTime", QDateTime::currentDateTime());
              query.bindValue(":notebook", currentNotebookId);
              query.bindValue(":text", text);
              if(query.exec())  {
                  std::cout  << "note added\n";
              } else    {
                  std::cout  << "error saving note to database\n";
                  qDebug() << query.lastQuery() << query.lastError() << db.lastError();
              }
          }
          exit(0);
      }

      if(args.at(0) == "delete")   {
          if(args.length() < 2) {
              std::cout  << "no note to delete...\n";
          }
          else {
              qInfo() << args.at(1);
              QString intString = args.at(1);
              int noteId = QVariant(intString).toInt();
              if(noteId)    {
                  query.prepare(QString("delete from %1 where id=:id").arg(noteTable));
                  query.bindValue(":id", noteId);
                  if(query.exec())  {
                      std::cout  << "note deleted\n";
                  } else    {
                      qDebug() << query.lastQuery() << query.lastError() << db.lastError();
                      std::cout  << "error deleting note from database\n";
                  }
              }    else    {
                  std::cout  << "invalid note identifier\n";
              }
          }
          exit(0);
      }

    return app.exec();
}
