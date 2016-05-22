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
    QString noteTable = "note";
    QCoreApplication::setApplicationName("clignote");
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
            if(!query.exec(QString("CREATE TABLE %1(id INTEGER PRIMARY KEY, created_at DATETIME, text TEXT);").arg(noteTable)))    {
                qDebug() << query.lastQuery() << query.lastError() << db.lastError();
                std::cout  << "error creating database";
                exit(-1);
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
      if(!args.length())    {
          std::cout  << "argument required, use --help";
          exit(0);
      }
      // source is args.at(0), destination is args.at(1)

      if(args.at(0) == "list")  {
            if(query.exec(QString("select * from %1").arg(noteTable)))    {;
                if(query.first())   {
                    QSqlRecord record;
                    std::cout  << "  id   |    creation date    |  note \n";
                    std::cout  << QString("").fill('-',80).toStdString() << "\n";
                    do  {
                        record = query.record();
                        std::cout  << QString().fill(' ', 6-record.value(0).toString().length()).toStdString()
                                << record.value(0).toString().toStdString()
                                << " | "
                                << record.value(1).toDateTime().toString(Qt::ISODate).toStdString()
                                << " | "
                                << record.value(2).toString().toStdString()
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
              qDebug() << args.mid(1).join(" ");
              QString text = args.mid(1).join(" ");
              query.prepare(QString("insert into %1 values(NULL, :currentDateTime, :text)").arg(noteTable));
              query.bindValue(":currentDateTime", QDateTime::currentDateTime());
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
