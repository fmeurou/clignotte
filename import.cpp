#include "import.h"
#include "common.h"
#include "sql.h"

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QJsonValue>
#include <QProcess>
#include <QSqlError>
#include <QStandardPaths>
#include <QStringList>
#include <QTemporaryDir>
#include <QTextStream>
#include <QTime>
#include <QUuid>
#include <cstdio>

static bool isAudioFile(const QString &path)   {
    static const QStringList exts = {
        "mp3", "wav", "ogg", "oga", "opus", "m4a", "mp4", "mpeg", "mpga",
        "flac", "aac", "webm", "wma", "amr", "mka", "3gp"
    };
    return exts.contains(QFileInfo(path).suffix().toLower());
}

static bool isOcrFile(const QString &path)   {
    static const QStringList exts = {
        "png", "jpg", "jpeg", "tiff", "tif", "bmp",
        "pnm", "pbm", "pgm", "ppm", "webp", "pdf"
    };
    return exts.contains(QFileInfo(path).suffix().toLower());
}

static QStringList runTesseract(const QString &filePath)   {
    QProcess proc;
    proc.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    proc.start("tesseract", {filePath, "stdout"});
    if(!proc.waitForStarted(5000) || !proc.waitForFinished(-1) ||
       proc.exitStatus() != QProcess::NormalExit || proc.exitCode() != 0)
        return {};
    return QString::fromUtf8(proc.readAllStandardOutput()).split('\n');
}

static void importOcr(QSqlQuery &query, QSqlDatabase db, const QString &path, const QMap<QString, QString> &currentNotebook)   {
    if(!QFileInfo::exists(path))   {
        out << "file not found: " << path << "\n";
        out.flush();
        return;
    }
    if(QStandardPaths::findExecutable("tesseract").isEmpty())   {
        out << "tesseract not found. Install it with: pacman -S tesseract tesseract-data-eng\n";
        out.flush();
        return;
    }

    QStringList allLines;
    bool isPdf = QFileInfo(path).suffix().toLower() == "pdf";

    if(isPdf)   {
        if(QStandardPaths::findExecutable("pdftoppm").isEmpty())   {
            out << "pdftoppm not found (required for PDF OCR). Install it with: pacman -S poppler\n";
            out.flush();
            return;
        }
        QTemporaryDir tempDir;
        if(!tempDir.isValid())   {
            out << "unable to create temporary directory\n";
            out.flush();
            return;
        }
        out << "converting PDF pages with pdftoppm...\n";
        out.flush();
        QProcess pdftoppm;
        pdftoppm.setProcessChannelMode(QProcess::ForwardedErrorChannel);
        pdftoppm.start("pdftoppm", {"-png", path, QDir(tempDir.path()).filePath("page")});
        pdftoppm.waitForStarted(5000);
        pdftoppm.waitForFinished(-1);
        if(pdftoppm.exitStatus() != QProcess::NormalExit || pdftoppm.exitCode() != 0)   {
            out << "pdftoppm failed (exit code " << pdftoppm.exitCode() << ")\n";
            out.flush();
            return;
        }
        QStringList pages = QDir(tempDir.path()).entryList({"page-*.png"}, QDir::Files, QDir::Name);
        out << "OCR-ing " << pages.size() << " page(s) with tesseract...\n";
        out.flush();
        for(const QString &page : pages)
            allLines += runTesseract(QDir(tempDir.path()).filePath(page));
    }   else    {
        out << "running tesseract on " << path << "...\n";
        out.flush();
        allLines = runTesseract(path);
    }

    int count = 0;
    db.transaction();
    query.prepare(QString(INSERT_NOTE));
    for(const QString &line : allLines)   {
        if(line.trimmed().isEmpty()) continue;
        query.bindValue(":currentDateTime", QDateTime::currentDateTime());
        query.bindValue(":notebook", currentNotebook["id"]);
        query.bindValue(":text", line.trimmed());
        query.bindValue(":uuid", QUuid::createUuid().toString(QUuid::WithoutBraces));
        if(!query.exec())   {
            qCritical() << "error importing line:" << line;
            qDebug() << query.lastError() << db.lastError();
            db.rollback();
            return;
        }
        count++;
    }
    db.commit();
    out << count << " note(s) imported from OCR into notebook '" << currentNotebook["name"] << "'\n";
    out.flush();
}

static void importAudio(QSqlQuery &query, QSqlDatabase db, const QString &path, const QMap<QString, QString> &currentNotebook)    {
    if(!QFileInfo::exists(path))   {
        out << "file not found: " << path << "\n";
        out.flush();
        return;
    }
    QTemporaryDir tempDir;
    if(!tempDir.isValid())   {
        out << "unable to create temporary directory\n";
        out.flush();
        return;
    }
    out << "transcribing " << path << " with whisper (this may take a while)...\n";
    out.flush();

    QProcess whisper;
    whisper.setProcessChannelMode(QProcess::ForwardedErrorChannel);
    whisper.start("whisper", QStringList()
        << path
        << "--output_format" << "json"
        << "--output_dir" << tempDir.path());
    if(!whisper.waitForStarted(5000))   {
        out << "unable to start whisper. Install it with: pacman -S python-openai-whisper\n";
        out.flush();
        return;
    }
    whisper.waitForFinished(-1);
    if(whisper.exitStatus() != QProcess::NormalExit || whisper.exitCode() != 0)   {
        out << "whisper failed (exit code " << whisper.exitCode() << ")\n";
        out.flush();
        return;
    }

    QString jsonPath = QDir(tempDir.path()).filePath(QFileInfo(path).completeBaseName() + ".json");
    QFile jsonFile(jsonPath);
    if(!jsonFile.open(QIODevice::ReadOnly))   {
        out << "whisper output not found at " << jsonPath << "\n";
        out.flush();
        return;
    }
    QJsonParseError perr;
    QJsonDocument doc = QJsonDocument::fromJson(jsonFile.readAll(), &perr);
    if(doc.isNull())   {
        out << "failed to parse whisper output: " << perr.errorString() << "\n";
        out.flush();
        return;
    }
    QJsonArray segments = doc.object().value("segments").toArray();
    if(segments.isEmpty())   {
        out << "whisper produced no segments\n";
        out.flush();
        return;
    }

    int count = 0;
    db.transaction();
    query.prepare(QString(INSERT_NOTE));
    for(const QJsonValue &v : segments)   {
        QString text = v.toObject().value("text").toString().trimmed();
        if(text.isEmpty()) continue;
        double startSec = v.toObject().value("start").toDouble();
        QString stamp = QTime(0, 0).addMSecs(int(startSec * 1000)).toString("HH:mm:ss");
        QString noteText = QString("[%1] %2").arg(stamp, text);
        query.bindValue(":currentDateTime", QDateTime::currentDateTime());
        query.bindValue(":notebook", currentNotebook["id"]);
        query.bindValue(":text", noteText);
        query.bindValue(":uuid", QUuid::createUuid().toString(QUuid::WithoutBraces));
        if(!query.exec())   {
            out << "error inserting segment: " << query.lastError().text() << "\n";
            out.flush();
            db.rollback();
            return;
        }
        count++;
    }
    db.commit();
    out << count << " note(s) imported from audio into notebook '" << currentNotebook["name"] << "'\n";
    out.flush();
}

static int importTextStream(QSqlQuery &query, QSqlDatabase &db, QTextStream &in, const QMap<QString, QString> &currentNotebook)    {
    int count = 0;
    db.transaction();
    query.prepare(QString(INSERT_NOTE));
    while(!in.atEnd())   {
        QString line = in.readLine();
        if(line.trimmed().isEmpty()) continue;
        query.bindValue(":currentDateTime", QDateTime::currentDateTime());
        query.bindValue(":notebook", currentNotebook["id"]);
        query.bindValue(":text", line);
        query.bindValue(":uuid", QUuid::createUuid().toString(QUuid::WithoutBraces));
        if(!query.exec())   {
            qCritical() << "error importing line:" << line;
            qDebug() << query.lastError() << db.lastError();
            db.rollback();
            return -1;
        }
        count++;
    }
    db.commit();
    return count;
}

void importNotes(QSqlQuery &query, QSqlDatabase db, const QString &path, const QMap<QString, QString> &currentNotebook)    {
    if(path == "-")   {
        QTextStream in(stdin);
        int count = importTextStream(query, db, in, currentNotebook);
        if(count >= 0)   {
            out << count << " note(s) imported from stdin into notebook '" << currentNotebook["name"] << "'\n";
            out.flush();
        }
        return;
    }
    if(isAudioFile(path))   {
        importAudio(query, db, path, currentNotebook);
        return;
    }
    if(isOcrFile(path))   {
        importOcr(query, db, path, currentNotebook);
        return;
    }
    QFile file(path);
    if(!file.open(QIODevice::ReadOnly | QIODevice::Text))   {
        out << "unable to open file: " << path << " - " << file.errorString() << "\n";
        out.flush();
        return;
    }
    QTextStream in(&file);
    int count = importTextStream(query, db, in, currentNotebook);
    if(count >= 0)   {
        out << count << " note(s) imported into notebook '" << currentNotebook["name"] << "'\n";
        out.flush();
    }
}
