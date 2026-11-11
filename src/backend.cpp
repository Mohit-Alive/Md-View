#include "backend.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>

#include "filepicker.h"

Backend::Backend(FilePicker *picker, QObject *parent, const QStringList &editorCommand)
    : QObject(parent), m_picker(picker), m_editorCommand(editorCommand) {
    if (m_editorCommand.isEmpty())
        m_editorCommand = {"omarchy-launch-editor"};

    // Editors save by writing a new file and renaming it over the old one,
    // which ends the watch and fires several events in a row. Wait for the
    // dust to settle, then reload and re-arm.
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(120);
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Backend::reload);

    if (m_picker) {
        connect(m_picker, &FilePicker::openSelected, this, [this](const QUrl &url) {
            m_history.clear();
            open(url.toString());
        });
        connect(m_picker, &FilePicker::failed, this, &Backend::setError);
    }
}

QString Backend::fileName() const {
    return m_path.isEmpty() ? QString() : QFileInfo(m_path).fileName();
}

QString Backend::baseUrl() const {
    if (m_path.isEmpty())
        return {};
    QString dir = QFileInfo(m_path).absolutePath();
    if (!dir.endsWith('/'))
        dir += '/';
    return QUrl::fromLocalFile(dir).toString(QUrl::FullyEncoded);
}

bool Backend::isMarkdownUrl(const QString &url) {
    const QUrl parsed(url);
    const QString path = parsed.isLocalFile() ? parsed.toLocalFile() : url;
    static const QStringList suffixes{"md", "markdown", "mdown", "mkd", "mkdn", "mdx"};
    return suffixes.contains(QFileInfo(path).suffix().toLower());
}

bool Backend::open(const QString &pathOrUrl) {
    const QUrl url(pathOrUrl);
    QString path = url.isLocalFile() ? url.toLocalFile() : pathOrUrl;
    if (path.startsWith("~/"))
        path = QDir::homePath() + path.mid(1);
    return load(QFileInfo(path).absoluteFilePath(), false);
}

bool Backend::follow(const QString &url) {
    // Recorded before loading, so the change notification already carries
    // the new Back state.
    const QString previous = m_path;
    if (!previous.isEmpty())
        m_history.append(previous);
    const bool opened = open(url);
    if (!previous.isEmpty() && (!opened || m_path == previous))
        m_history.removeLast();
    return opened;
}

bool Backend::goBack() {
    while (!m_history.isEmpty()) {
        if (load(m_history.takeLast(), false))
            return true;
    }
    return false;
}

void Backend::reload() {
    if (m_path.isEmpty())
        return;
    // A save in progress can leave the file missing for a moment; keep showing
    // the last good version and say so only if it stays gone.
    if (!QFileInfo::exists(m_path)) {
        setMissing(true);
        watch();
        return;
    }
    load(m_path, true);
}

bool Backend::load(const QString &path, bool reload) {
    const QFileInfo info(path);
    QString problem;
    QByteArray data;
    if (!info.exists())
        problem = tr("%1 does not exist.").arg(path);
    else if (info.isDir())
        problem = tr("%1 is a folder, not a Markdown file.").arg(path);
    else if (info.size() > MaxBytes)
        problem = tr("%1 is too large to show (over 32 MB).").arg(info.fileName());
    else {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly))
            problem = tr("Could not read %1: %2").arg(info.fileName(), file.errorString());
        else
            data = file.readAll();
    }

    if (!problem.isEmpty()) {
        // A failed live reload keeps the document that's already showing.
        if (reload) {
            setMissing(!info.exists());
            watch();
        } else {
            setError(problem);
        }
        return false;
    }

    const QString text = QString::fromUtf8(data);
    const bool sameFile = path == m_path;
    setMissing(false);
    if (reload && sameFile && text == m_text) {
        watch();
        return true;
    }

    m_path = path;
    m_text = text;
    m_reloaded = reload && sameFile;
    ++m_revision;
    setError({});
    watch();
    emit documentChanged();
    return true;
}

void Backend::watch() {
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);
    if (m_path.isEmpty())
        return;
    // The folder catches the rename editors save with, and a file that comes
    // back after being replaced.
    const QString dir = QFileInfo(m_path).absolutePath();
    if (QFileInfo::exists(dir))
        m_watcher.addPath(dir);
    if (QFileInfo::exists(m_path))
        m_watcher.addPath(m_path);
}

void Backend::chooseFile() {
    if (!m_picker) {
        setError(tr("No file picker is available."));
        return;
    }
    m_picker->openMarkdown(m_path.isEmpty() ? QDir::homePath() : QFileInfo(m_path).absolutePath());
}

void Backend::openInEditor() {
    if (m_path.isEmpty())
        return;
    const QString program = QStandardPaths::findExecutable(m_editorCommand.first());
    if (program.isEmpty()) {
        setError(tr("Could not find %1 to open the editor.").arg(m_editorCommand.first()));
        return;
    }
    if (!QProcess::startDetached(program, m_editorCommand.mid(1) + QStringList{m_path}))
        setError(tr("Could not start %1.").arg(m_editorCommand.first()));
}

void Backend::clearError() {
    setError({});
}

void Backend::setError(const QString &error) {
    if (error == m_error)
        return;
    m_error = error;
    emit errorChanged();
}

void Backend::setMissing(bool missing) {
    if (missing == m_missing)
        return;
    m_missing = missing;
    emit missingChanged();
}
