#pragma once

#include <QFileSystemWatcher>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QUrl>

class FilePicker;

// The open Markdown document: reads it, watches it for changes so the view
// reloads live, and remembers the files followed through links for Back.
class Backend : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString filePath READ filePath NOTIFY documentChanged)
    Q_PROPERTY(QString fileName READ fileName NOTIFY documentChanged)
    Q_PROPERTY(QString baseUrl READ baseUrl NOTIFY documentChanged)
    Q_PROPERTY(QString text READ text NOTIFY documentChanged)
    Q_PROPERTY(int revision READ revision NOTIFY documentChanged)
    Q_PROPERTY(bool reloaded READ reloaded NOTIFY documentChanged)
    Q_PROPERTY(bool missing READ missing NOTIFY missingChanged)
    Q_PROPERTY(bool canGoBack READ canGoBack NOTIFY documentChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)

public:
    // The picker and the editor command are injectable so tests need neither
    // a portal nor a terminal.
    explicit Backend(FilePicker *picker = nullptr, QObject *parent = nullptr,
                     const QStringList &editorCommand = {});

    QString filePath() const { return m_path; }
    QString fileName() const;
    QString baseUrl() const;
    QString text() const { return m_text; }
    int revision() const { return m_revision; }
    bool reloaded() const { return m_reloaded; }
    bool missing() const { return m_missing; }
    bool canGoBack() const { return !m_history.isEmpty(); }
    QString error() const { return m_error; }

    // Opens a path or a file:// URL. Returns false and sets error when it
    // can't be read.
    Q_INVOKABLE bool open(const QString &pathOrUrl);
    // Opens a document reached through a link, so Back returns to this one.
    Q_INVOKABLE bool follow(const QString &url);
    Q_INVOKABLE bool goBack();
    Q_INVOKABLE void reload();
    Q_INVOKABLE void chooseFile();
    Q_INVOKABLE void openInEditor();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE static bool isMarkdownUrl(const QString &url);

    static constexpr qint64 MaxBytes = 32 * 1024 * 1024;

signals:
    void documentChanged();
    void missingChanged();
    void errorChanged();

private:
    bool load(const QString &path, bool reload);
    void watch();
    void setError(const QString &error);
    void setMissing(bool missing);

    FilePicker *m_picker = nullptr;
    QStringList m_editorCommand;
    QString m_path;
    QString m_text;
    int m_revision = 0;
    bool m_reloaded = false;
    bool m_missing = false;
    QString m_error;
    QStringList m_history;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
};
