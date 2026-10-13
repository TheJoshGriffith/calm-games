#include <QDebug>
#include <QDesktopServices>
#include <QFile>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QQuickWindow>
#include <QTimer>
#include <QUrl>

Q_IMPORT_QML_PLUGIN(CalmPlugin)

namespace {

// CALM_SMOKE_TEST=1 loads the UI, runs briefly and fails on any QML warning.
int g_qmlWarnings = 0;
QtMessageHandler g_previousHandler = nullptr;
// CALM_SMOKE_LOG=<file> also appends every message there (iOS simulator CI,
// where the app's console isn't easy to capture).
QFile* g_smokeLog = nullptr;

void countingHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (type != QtDebugMsg && type != QtInfoMsg && QString::fromUtf8(context.category ? context.category : "").startsWith(QLatin1String("qt.qml")))
        ++g_qmlWarnings;
    else if (type == QtWarningMsg && (message.contains(QLatin1String(".qml:")) || message.contains(QLatin1String("qrc:/"))))
        ++g_qmlWarnings;
    if (g_smokeLog) {
        g_smokeLog->write(qFormatLogMessage(type, context, message).toUtf8() + '\n');
        g_smokeLog->flush();
    }
    g_previousHandler(type, context, message);
}

// Hands share links to Main.qml's openLink(), which opens the shared board
// (or says the link holds none) and shows the game.
class ShareLinks : public QObject {
    Q_OBJECT

public:
    explicit ShareLinks(QQmlApplicationEngine& engine)
        : m_engine(engine)
    {
    }

    bool open(const QString& link)
    {
        QVariant opened;
        if (QObject* root = m_engine.rootObjects().value(0))
            QMetaObject::invokeMethod(root, "openLink", Q_RETURN_ARG(QVariant, opened), Q_ARG(QVariant, link));
        qInfo("share link: %s %s", opened.toBool() ? "opened" : "rejected", qPrintable(link));
        return opened.toBool();
    }

public slots:
    // QDesktopServices URL handler for https: iOS hands universal links
    // (https://sudoku.gwynant.app/s/...) to it, once the app has the
    // associated-domains entitlement. Any other https URL the app itself
    // opens still goes to the browser.
    void openUrl(const QUrl& url)
    {
        if (url.path().startsWith(QLatin1String("/s/"))) {
            open(url.toString());
            return;
        }
        QDesktopServices::unsetUrlHandler(QStringLiteral("https"));
        QDesktopServices::openUrl(url);
        QDesktopServices::setUrlHandler(QStringLiteral("https"), this, "openUrl");
    }

private:
    QQmlApplicationEngine& m_engine;
};

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("CalmGames"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("calmgames.app"));
    QGuiApplication::setApplicationName(QStringLiteral("Sudoku"));

    const bool smokeTest = qEnvironmentVariableIsSet("CALM_SMOKE_TEST");
    if (smokeTest) {
        if (qEnvironmentVariableIsSet("CALM_SMOKE_LOG")) {
            g_smokeLog = new QFile(qEnvironmentVariable("CALM_SMOKE_LOG"));
            if (!g_smokeLog->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
                delete g_smokeLog;
                g_smokeLog = nullptr;
            }
        }
        g_previousHandler = qInstallMessageHandler(countingHandler);
    }

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Sudoku/qml/Main.qml")));
    if (engine.rootObjects().isEmpty()) {
        qCritical("Main.qml failed to load");
        return 1;
    }

    // Share links: the web build starts the app with --share <payload> for
    // https://<domain>/s/<payload> (any build accepts it, with a payload or a
    // whole link); on iOS, universal links arrive through the URL handler.
    ShareLinks shareLinks(engine);
    const QStringList args = QCoreApplication::arguments();
    const int share = args.indexOf(QStringLiteral("--share"));
    if (share >= 0 && share + 1 < args.size())
        shareLinks.open(args.at(share + 1));
#ifndef Q_OS_WASM
    QDesktopServices::setUrlHandler(QStringLiteral("https"), &shareLinks, "openUrl");
#endif

    // Development aid: --size <width>x<height> resizes the window (to check
    // layouts such as a desktop browser's).
    const int size = args.indexOf(QStringLiteral("--size"));
    if (size >= 0 && size + 1 < args.size() && !engine.rootObjects().isEmpty()) {
        const QStringList wh = args.at(size + 1).split(QLatin1Char('x'));
        if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst()); window && wh.size() == 2)
            window->resize(wh.at(0).toInt(), wh.at(1).toInt());
    }

    // Development aid: --screenshot <file.png> saves the window after start-up
    // (combine with --play <Level>, --settings or --stats to open that screen).
    const int shot = args.indexOf(QStringLiteral("--screenshot"));
    if (shot >= 0 && shot + 1 < args.size()) {
        const QString path = args.at(shot + 1);
        QTimer::singleShot(3000, &app, [&engine, path] {
            if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().constFirst()))
                window->grabWindow().save(path);
            QCoreApplication::quit();
        });
    }

    if (smokeTest) {
        QTimer::singleShot(2500, &app, [] {
            if (g_qmlWarnings > 0)
                qCritical("smoke test: %d QML warning(s)", g_qmlWarnings);
            else
                qInfo("smoke test: ok");
            QCoreApplication::exit(g_qmlWarnings > 0 ? 1 : 0);
        });
    }
    const int status = app.exec();
#ifndef Q_OS_WASM
    QDesktopServices::unsetUrlHandler(QStringLiteral("https"));
#endif
    return status;
}

#include "main.moc"
