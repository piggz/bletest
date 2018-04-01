#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>

#include "bluetoothleinterface.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);

    QQmlApplicationEngine engine;

    BluetoothLEInterface leInterface;

    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    engine.rootContext()->setContextProperty("LEInterface", &leInterface);
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
