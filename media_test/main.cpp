#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QtQml>
#include "media_player.h"
#include "ximageprovider.h"

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    QGuiApplication app(argc, argv);

    XImageProvider *xImageProvider = new XImageProvider();
    qmlRegisterType<mediaPlayer>("karl.media", 1, 0, "MediaPlayer");

    QQmlApplicationEngine engine;

    engine.rootContext()->setContextProperty("xImageProvider_0", xImageProvider);
    engine.addImageProvider(QLatin1String("xProvider_0"), xImageProvider->provider);

    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    if (engine.rootObjects().isEmpty())
        return -1;

    return app.exec();
}
