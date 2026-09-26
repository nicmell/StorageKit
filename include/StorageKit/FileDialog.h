// SPDX-License-Identifier: MIT
#pragma once

#include <QFuture>
#include <QStringList>
#include <QUrl>

namespace StorageKit {

/*!
 * Pickers mirroring QFileDialog's static getters, asynchronous because
 * Android's SAF pickers are (results come back via activity results).
 * Qt's platform helper shows the native dialog on every platform; on Android
 * a persistable URI permission is taken on each result, so granted locations
 * survive app restarts — build a Dir from getExistingDirectoryUrl()'s result,
 * a File from the others. A cancelled pick yields an empty url / empty list.
 *
 * QFileDialog-based: applications must run a QApplication.
 */
class FileDialog
{
public:
    static QFuture<QUrl> getExistingDirectoryUrl();
    static QFuture<QUrl> getOpenFileUrl(const QStringList &mimeTypes = {});
    static QFuture<QList<QUrl>> getOpenFileUrls(const QStringList &mimeTypes = {});
    // The document is created (ACTION_CREATE_DOCUMENT on Android).
    static QFuture<QUrl> getSaveFileUrl(const QString &suggestedName,
                                        const QString &mimeType = {});

    // Drop a persisted grant taken by the pickers (no-op on desktop).
    static void releaseGrant(const QUrl &url);

private:
    FileDialog() = delete;
};

} // namespace StorageKit
