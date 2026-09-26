// SPDX-License-Identifier: MIT
#pragma once

#include <StorageKit/FileInfo.h>

#include <QUrl>

#include <memory>

namespace StorageKit {

// stat() result used between the backends and the public classes.
struct Stat
{
    enum Type { File, Dir };
    Type type = File;
    qint64 size = 0;
    QDateTime mtime;
    QString mime;
    bool readable = false;
    bool writable = false;
};

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

// Per-platform free functions (src/local/ vs src/android/) backing File and
// FileDialog; the dialogs themselves are platform-independent (src/filedialog.cpp).
std::shared_ptr<Backend> createBackend(const QUrl &root);
int openUrl(const QUrl &url, int flags);
FileInfo urlInfo(const QUrl &url);
bool removeUrl(const QUrl &url);
// Renamed document's new url; empty on failure.
QUrl renameUrl(const QUrl &url, const QString &newName);
void releaseGrant(const QUrl &url);

} // namespace StorageKit
