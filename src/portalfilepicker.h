// Adapted from Omacut's src/portalfilepicker.h (MIT, David Heinemeier Hansson).
#pragma once

#include <QVariantMap>

#include "filepicker.h"

class QDBusPendingCallWatcher;

// Opens files through xdg-desktop-portal's FileChooser over D-Bus, so the
// dialog is the desktop's own.
class PortalFilePicker : public FilePicker {
    Q_OBJECT

public:
    explicit PortalFilePicker(QObject *parent = nullptr);
    ~PortalFilePicker() override;

    void openMarkdown(const QString &startFolder) override;

private slots:
    void handleResponse(uint response, const QVariantMap &results);

private:
    void requestFile(const QString &title, QVariantMap options);
    bool connectToRequestPath(const QString &path);
    void clearPending();
    // Best-effort Close of the pending portal request, if there is one.
    void closePendingRequest();

    QString m_pendingPath;
    bool m_pending = false;
    // Bumped per request, so a late reply for a replaced request is ignored.
    int m_pendingGeneration = 0;
    QDBusPendingCallWatcher *m_pendingWatcher = nullptr;
};
