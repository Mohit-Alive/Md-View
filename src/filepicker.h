// The file picker interface follows Omacut (MIT, David Heinemeier Hansson),
// so tests can hand the backend a fake picker.
#pragma once

#include <QObject>
#include <QUrl>

class FilePicker : public QObject {
    Q_OBJECT

public:
    explicit FilePicker(QObject *parent = nullptr) : QObject(parent) {}
    virtual ~FilePicker() = default;

    // startFolder is where the dialog opens: the current file's folder, or home.
    virtual void openMarkdown(const QString &startFolder) = 0;

signals:
    void openSelected(const QUrl &url);
    void failed(const QString &message);
};
