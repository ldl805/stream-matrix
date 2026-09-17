#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickWindow>
#include <QTranslator>
#include <QKeyEvent>
#include <QSocketNotifier>
#include <QTimer>
#include <csignal>
#include <sys/socket.h>
#include <unistd.h>

#include "qmlavplayer.h"
#include "context.h"
#include "eventfilter.h"
#include "clipboard.h"
#include "singleapplication.h"
#include "viewportslayoutscollectionmodel.h"

static int sigintFd[2];
static int sigtermFd[2];

static void posixSignalHandler(int sig)
{
    char a = 1;
    if (sig == SIGINT) {
        [[maybe_unused]] auto res = ::write(sigintFd[0], &a, sizeof(a));
    } else if (sig == SIGTERM) {
        [[maybe_unused]] auto res = ::write(sigtermFd[0], &a, sizeof(a));
    }
}

class QuitShortcutFilter : public QObject
{
public:
    explicit QuitShortcutFilter(QObject *parent = nullptr) : QObject(parent) {}

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (event->type() == QEvent::KeyPress || event->type() == QEvent::ShortcutOverride) {
            auto *keyEvent = static_cast<QKeyEvent *>(event);
            const int key = keyEvent->key();
            const Qt::KeyboardModifiers cleanMods = keyEvent->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier | Qt::MetaModifier);

            bool isQuit = false;

            // Quit shortcuts: Ctrl+Q, Ctrl+W, Ctrl+C (with or without Shift)
            if ((cleanMods & Qt::ControlModifier) && (key == Qt::Key_Q || key == Qt::Key_W || key == Qt::Key_C)) {
                isQuit = true;
            }
            // Alt+F4
            else if ((cleanMods & Qt::AltModifier) && key == Qt::Key_F4) {
                isQuit = true;
            }
            // 'q' or 'Q' when no Ctrl/Alt/Meta modifier and focus is NOT inside a text input field
            else if ((cleanMods == Qt::NoModifier || cleanMods == Qt::ShiftModifier) && (key == Qt::Key_Q)) {
                QObject *focus = QGuiApplication::focusObject();
                bool isTextInput = false;
                if (focus) {
                    const char *className = focus->metaObject()->className();
                    if (focus->inherits("QQuickTextInput") ||
                        focus->inherits("QQuickTextEdit") ||
                        (className && (strstr(className, "TextInput") || strstr(className, "TextEdit") || strstr(className, "TextField")))) {
                        isTextInput = true;
                    }
                }
                if (!isTextInput) {
                    isQuit = true;
                }
            }

            if (isQuit) {
                event->accept();
                qInfo() << "Quit hotkey pressed (" << key << "), shutting down StreamMatrix...";
                QGuiApplication::quit();
                return true;
            }
        }
        return QObject::eventFilter(watched, event);
    }
};

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

    QuitShortcutFilter quitFilter;
    app.installEventFilter(&quitFilter);

    if (::socketpair(AF_UNIX, SOCK_STREAM, 0, sigintFd) == 0 &&
        ::socketpair(AF_UNIX, SOCK_STREAM, 0, sigtermFd) == 0) {
        auto *snInt = new QSocketNotifier(sigintFd[1], QSocketNotifier::Read, &app);
        QObject::connect(snInt, &QSocketNotifier::activated, &app, [snInt]() {
            snInt->setEnabled(false);
            char a;
            [[maybe_unused]] auto res = ::read(sigintFd[1], &a, sizeof(a));
            qInfo() << "SIGINT received, shutting down StreamMatrix...";
            QGuiApplication::quit();
        });
        auto *snTerm = new QSocketNotifier(sigtermFd[1], QSocketNotifier::Read, &app);
        QObject::connect(snTerm, &QSocketNotifier::activated, &app, [snTerm]() {
            snTerm->setEnabled(false);
            char a;
            [[maybe_unused]] auto res = ::read(sigtermFd[1], &a, sizeof(a));
            qInfo() << "SIGTERM received, shutting down StreamMatrix...";
            QGuiApplication::quit();
        });

        struct sigaction sa;
        sa.sa_handler = posixSignalHandler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sigaction(SIGINT, &sa, nullptr);
        sigaction(SIGTERM, &sa, nullptr);
    }

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
    QObject::connect(&engine, &QQmlApplicationEngine::quit, &app, &QGuiApplication::quit);
    engine.load(url);

    if (qEnvironmentVariableIsSet("STREAM_MATRIX_TEST_KEY_QUIT")) {
        QTimer::singleShot(1500, &app, [&app]() {
            qInfo() << "Simulating Ctrl+Q KeyPress event for test...";
            QKeyEvent keyEvent(QEvent::KeyPress, Qt::Key_Q, Qt::ControlModifier, QStringLiteral("q"));
            QCoreApplication::sendEvent(&app, &keyEvent);
        });
    }

    return app.exec();
}
