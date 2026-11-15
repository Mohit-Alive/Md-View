#pragma once

#include <QWebEngineUrlSchemeHandler>

// Serves images, video, and audio beside the Markdown file to the rendered
// page as mdview-file:///absolute/path. Chromium refuses file:// media in a
// page bundled with the app, and only media is served, so a document can't
// use this to read anything else off the disk.
class LocalFiles : public QWebEngineUrlSchemeHandler {
    Q_OBJECT

public:
    static constexpr const char *Scheme = "mdview-file";

    // Must run before the web engine starts.
    static void registerScheme();
    // The type served for a path, or empty when the file is not media.
    static QString mediaType(const QString &path);

    using QWebEngineUrlSchemeHandler::QWebEngineUrlSchemeHandler;
    void requestStarted(QWebEngineUrlRequestJob *job) override;
};
