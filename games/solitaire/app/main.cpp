#include <QDebug>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlExtensionPlugin>
#include <QQuickWindow>
#include <QTimer>

Q_IMPORT_QML_PLUGIN(CalmPlugin)

namespace {

// CALM_SMOKE_TEST=1 loads the UI, runs briefly and fails on any QML warning.
int g_qmlWarnings = 0;
QtMessageHandler g_previousHandler = nullptr;

void countingHandler(QtMsgType type, const QMessageLogContext& context, const QString& message)
{
    if (type != QtDebugMsg && type != QtInfoMsg && QString::fromUtf8(context.category ? context.category : "").startsWith(QLatin1String("qt.qml")))
        ++g_qmlWarnings;
    else if (type == QtWarningMsg && (message.contains(QLatin1String(".qml:")) || message.contains(QLatin1String("qrc:/"))))
        ++g_qmlWarnings;
    g_previousHandler(type, context, message);
}

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);
    QGuiApplication::setOrganizationName(QStringLiteral("CalmGames"));
    QGuiApplication::setOrganizationDomain(QStringLiteral("calmgames.app"));
    QGuiApplication::setApplicationName(QStringLiteral("Solitaire"));

    const bool smokeTest = qEnvironmentVariableIsSet("CALM_SMOKE_TEST");
    if (smokeTest)
        g_previousHandler = qInstallMessageHandler(countingHandler);

    QQmlApplicationEngine engine;
    engine.addImportPath(QStringLiteral("qrc:/qt/qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app, [] { QCoreApplication::exit(1); },
        Qt::QueuedConnection);
    engine.load(QUrl(QStringLiteral("qrc:/qt/qml/Solitaire/qml/Main.qml")));
    if (engine.rootObjects().isEmpty())
        return 1;

    // Development aid: --screenshot <file.png> saves the window after start-up
    // (combine with --play <Level> or --settings to open that screen).
    const QStringList args = QCoreApplication::arguments();
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
            QCoreApplication::exit(g_qmlWarnings > 0 ? 1 : 0);
        });
    }
    return app.exec();
}
