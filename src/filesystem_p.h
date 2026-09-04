// SPDX-License-Identifier: MIT
#pragma once

#include <StorageKit/FileSystem.h>

namespace StorageKit {

/*!
 * Backend contract. Paths arriving here are already validated and normalized
 * ("" means the root, otherwise "a/b/c" with clean segments). All methods
 * follow the unix convention: fd/0 on success, -1 + errno on error.
 */
class Backend
{
public:
    virtual ~Backend() = default;

    virtual bool isValid() const = 0;
    virtual QUrl root() const = 0;

    virtual int openFd(const QString &path, int flags) = 0;
    virtual int statOne(const QString &path, Stat *out) = 0;
    virtual int mkdir(const QString &path) = 0;
    virtual int rmdir(const QString &path) = 0;
    virtual int unlink(const QString &path) = 0;
    virtual int rename(const QString &from, const QString &to) = 0;
    virtual int truncate(const QString &path, qint64 length) = 0;
    virtual bool exists(const QString &path) = 0;
    // Unfiltered directory listing, no "."/".." entries.
    virtual int rawList(const QString &path, FileInfoList *out) = 0;
};

// Implemented once per platform (src/local/ vs src/android/).
std::shared_ptr<Backend> createBackend(const QUrl &root);
QFuture<FileSystem> platformPickFolder();
QFuture<QUrl> platformPickFile(const QStringList &mimeTypes);
QFuture<QList<QUrl>> platformPickFiles(const QStringList &mimeTypes);
int platformOpenUrl(const QUrl &url, int flags);
FileInfo platformUrlInfo(const QUrl &url);
void platformReleaseGrant(const QUrl &url);

} // namespace StorageKit
