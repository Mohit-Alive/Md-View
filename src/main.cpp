// mdview — shows Markdown files the way GitHub renders them, following the
// Omarchy theme and reloading live as the file changes.

#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWebEngineProfile>
#include <QUrl>
#include <QtWebEngineQuick>

#include "backend.h"
#include "localfiles.h"
#include "portalfilepicker.h"
#include "theme.h"
#include "translator.h"

int main(int argc, char *argv[]) {
    // Must run before the application object exists, so the web engine can
    // share its graphics context with Qt Quick.
    LocalFiles::registerScheme();
    QtWebEngineQuick::initialize();

    QGuiApplication app(argc, argv);
    app.setApplicationName("mdview");
    app.setApplicationDisplayName("MD View");
    app.setApplicationVersion("0.1.0");

    // Matches the window to mdview.desktop, so the compositor (Wayland app_id)
    // and the launcher pick up the installed icon.
    app.setDesktopFileName("mdview");
    app.setWindowIcon(QIcon::fromTheme("mdview"));

    QQuickStyle::setStyle("Material");

    // Installed before anything is translated, and every qsTr() binding is
    // re-evaluated when the Omarchy language changes.
    Translator translator;
    app.installTranslator(&translator);

    QCommandLineParser parser;
    parser.setApplicationDescription(QCoreApplication::translate("main", "Show Markdown files the way GitHub does."));
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addPositionalArgument("file", QCoreApplication::translate("main", "The Markdown file to show."), "[file]");
    parser.process(app);

    Theme theme;
    PortalFilePicker picker;
    Backend backend(&picker);
    if (!parser.positionalArguments().isEmpty())
        backend.open(parser.positionalArguments().first());

    QQmlApplicationEngine engine;
    QObject::connect(&translator, &Translator::changed, &engine, &QQmlEngine::retranslate);
    engine.rootContext()->setContextProperty("translator", &translator);
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    // Installed once the window exists: touching the default profile before
    // the first WebEngineView is created leaves the view drawing nothing.
    // The page asks for local media only after it has loaded, so this is
    // still in time.
    LocalFiles localFiles;
    QQuickWebEngineProfile::defaultProfile()->installUrlSchemeHandler(LocalFiles::Scheme, &localFiles);

    return app.exec();
}
