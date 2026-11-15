#include "localfiles.h"

#include <QFile>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QWebEngineUrlRequestJob>
#include <QWebEngineUrlScheme>

void LocalFiles::registerScheme() {
    QWebEngineUrlScheme scheme(Scheme);
    scheme.setSyntax(QWebEngineUrlScheme::Syntax::Path);
    scheme.setFlags(QWebEngineUrlScheme::SecureScheme);
    QWebEngineUrlScheme::registerScheme(scheme);
}

QString LocalFiles::mediaType(const QString &path) {
    const QString type = QMimeDatabase().mimeTypeForFile(path).name();
    const bool media = type.startsWith("image/") || type.startsWith("video/") || type.startsWith("audio/");
    return media ? type : QString();
}

void LocalFiles::requestStarted(QWebEngineUrlRequestJob *job) {
    const QString path = job->requestUrl().path();
    const QFileInfo info(path);
    if (!info.isFile() || !info.isReadable()) {
        job->fail(QWebEngineUrlRequestJob::UrlNotFound);
        return;
    }
    const QString type = mediaType(path);
    if (type.isEmpty()) {
        job->fail(QWebEngineUrlRequestJob::RequestDenied);
        return;
    }
    auto *file = new QFile(path, job);
    if (!file->open(QIODevice::ReadOnly)) {
        job->fail(QWebEngineUrlRequestJob::RequestFailed);
        return;
    }
    job->reply(type.toUtf8(), file);
}
