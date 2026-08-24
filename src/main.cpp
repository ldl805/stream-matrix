#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTranslator>

#include "qmlavplayer.h"
#include "context.h"
#include "eventfilter.h"
#include "clipboard.h"
#include "singleapplication.h"
#include "viewportslayoutscollectionmodel.h"

void registerQmlTypes()
{
    // StreamMatrix Namespaces
    qmlRegisterSingletonType<Context>("StreamMatrix.Core", 1, 0, "Context",
                                      []([[maybe_unused]] QQmlEngine *engine,
                                         [[maybe_unused]] QJSEngine *scriptEngine) -> QObject * {
        return new Context();
    });
    qmlRegisterSingletonType<Clipboard>("StreamMatrix.Utils", 1, 0, "Clipboard",
                                        []([[maybe_unused]] QQmlEngine *engine,
                                           [[maybe_unused]] QJSEngine *scriptEngine) -> QObject * {
        return new Clipboard();
    });
    qmlRegisterSingletonType<SingleApplication>("StreamMatrix.Utils", 1, 0, "SingleApplication",
                                                []([[maybe_unused]] QQmlEngine *engine,
                                                   [[maybe_unused]] QJSEngine *scriptEngine) -> QObject * {
        return new SingleApplication();
    });

    qmlRegisterType<QmlAVPlayer>("StreamMatrix.Multimedia", 1, 0, "QmlAVPlayer");
    qmlRegisterType<ViewportsLayoutItem>("StreamMatrix.Models", 1, 0, "ViewportsLayoutItem");
    qmlRegisterType<ViewportsLayoutModel>("StreamMatrix.Models", 1, 0, "ViewportsLayoutModel");
    qmlRegisterType<ViewportsLayoutsCollectionModel>("StreamMatrix.Models", 1, 0, "ViewportsLayoutsCollectionModel");
    qmlRegisterType<EventFilter>("StreamMatrix.Utils", 1, 0, "EventFilter");

    // Legacy CCTV_Viewer namespaces for backwards compatibility
    qmlRegisterSingletonType<Context>("CCTV_Viewer.Core", 1, 0, "Context",
                                      []([[maybe_unused]] QQmlEngine *engine,
                                         [[maybe_unused]] QJSEngine *scriptEngine) -> QObject * {
        return new Context();
    });
    qmlRegisterSingletonType<Clipboard>("CCTV_Viewer.Utils", 1, 0, "Clipboard",
                                        []([[maybe_unused]] QQmlEngine *engine,
                                           [[maybe_unused]] QJSEngine *scriptEngine) -> QObject * {
        return new Clipboard();
    });
    qmlRegisterSingletonType<SingleApplication>("CCTV_Viewer.Utils", 1, 0, "SingleApplication",
                                                []([[maybe_unused]] QQmlEngine *engine,
                                                   [[maybe_unused]] QJSEngine *scriptEngine) -> QObject * {
        return new SingleApplication();
    });

    qmlRegisterType<QmlAVPlayer>("CCTV_Viewer.Multimedia", 1, 0, "QmlAVPlayer");
    qmlRegisterType<ViewportsLayoutItem>("CCTV_Viewer.Models", 1, 0, "ViewportsLayoutItem");
    qmlRegisterType<ViewportsLayoutModel>("CCTV_Viewer.Models", 1, 0, "ViewportsLayoutModel");
    qmlRegisterType<ViewportsLayoutsCollectionModel>("CCTV_Viewer.Models", 1, 0, "ViewportsLayoutsCollectionModel");
    qmlRegisterType<EventFilter>("CCTV_Viewer.Utils", 1, 0, "EventFilter");
}

int main(int argc, char *argv[])
{
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

#if defined(APP_NAME)
    QCoreApplication::setApplicationName(QLatin1String(APP_NAME));
#endif
#if defined(APP_VERSION)
    QCoreApplication::setApplicationVersion(QLatin1String(APP_VERSION));
#endif
#if defined(ORG_NAME)
    QCoreApplication::setOrganizationName(QLatin1String(ORG_NAME));
#endif
#if defined(ORG_DOMAIN)
    QCoreApplication::setOrganizationDomain(QLatin1String(ORG_DOMAIN));
#endif

    qInfo() << "StreamMatrix version:" << APP_VERSION;

    registerQmlTypes();

    QGuiApplication app(argc, argv);
    QQmlApplicationEngine engine;
    QTranslator translator;
    const QString locale = QLocale::system().name();
    if (!translator.load(QLatin1String("stream-matrix_") + locale, QLatin1String(":/translations/"))) {
        translator.load(QLatin1String("cctv-viewer_") + locale, QLatin1String(":/translations/"));
    }
    app.installTranslator(&translator);
    app.setWindowIcon(QIcon(QLatin1String(":/images/stream-matrix.svg")));

    Context::init();

    engine.addImportPath(":/src/imports");
    const QUrl url(QStringLiteral("qrc:/src/RootWindow.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated, &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);
    engine.load(url);

    return app.exec();
}
