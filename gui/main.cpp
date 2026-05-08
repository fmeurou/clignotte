#include "dbcontroller.h"
#include "notebookmodel.h"
#include "notemodel.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setApplicationName("clignotte");
    QGuiApplication::setApplicationVersion("1.0");

    QQuickStyle::setStyle("Material");

    DbController controller;
    if (!controller.initialize()) return 2;

    qmlRegisterUncreatableType<NotebookModel>("Clignotte", 1, 0, "NotebookModel",
        "Created in C++");
    qmlRegisterUncreatableType<NoteModel>("Clignotte", 1, 0, "NoteModel",
        "Created in C++");

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("db", &controller);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) return 1;
    return app.exec();
}
