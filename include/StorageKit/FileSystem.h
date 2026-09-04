// SPDX-License-Identifier: MIT
#pragma once

#include <StorageKit/FileInfo.h>

#include <QDir>
#include <QFuture>
#include <QIODevice>
#include <QUrl>

#include <memory>

namespace StorageKit {

class Backend;

/*!
 * stat() result. Subset of struct stat that is honest on both desktop and
 * Android shared storage, plus the MIME type.
 */
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
 * A capability-style filesystem rooted at a user-granted directory:
 * a plain directory (file://) on desktop, a Storage Access Framework tree URI
 * (content://) on Android. Instances cannot reach outside their root.
 *
 * The constructor is synchronous and never shows UI; the pick*() factories are
 * the only entry points that do. Persist root() and rebuild with the
 * constructor to keep access across runs (the folder picker takes a
 * persistable URI permission on Android).
 *
 * I/O follows the unix idiom: open() returns a real native file descriptor
 * (use read/write/lseek/fstat/close from libc), mutating calls return 0 or -1
 * with errno set. Listing follows the Qt idiom via entryInfoList().
 *
 * Blocking API — do not call from the GUI thread on Android (binder IPC).
 */
class FileSystem
{
public:
    FileSystem(); // invalid instance, same as FileSystem(QUrl{})
    explicit FileSystem(const QUrl &root);

    bool isValid() const; // root reachable and (Android) grant still held
    QUrl root() const;

    // Async pickers. A cancelled pick yields an invalid FileSystem / empty
    // url / empty list respectively.
    static QFuture<FileSystem> pickFolder();
    static QFuture<QUrl> pickFile(const QStringList &mimeTypes = {});
    static QFuture<QList<QUrl>> pickFiles(const QStringList &mimeTypes = {});

    // fd for a pickFile()/pickFiles() result (single-file grant, no tree).
    static int openUrl(const QUrl &url, int flags);
    // Drop a persisted grant taken by pickFolder() (no-op on desktop).
    static void releaseGrant(const QUrl &url);

    // Unix surface. Paths are relative to root(); absolute paths and ".."
    // are rejected with EACCES. Returns fd/0 on success, -1 + errno on error.
    int open(const QString &path, int flags);
    int stat(const QString &path, Stat *out) const;
    int mkdir(const QString &path);
    int rmdir(const QString &path);
    int unlink(const QString &path);
    int rename(const QString &from, const QString &to);
    int truncate(const QString &path, qint64 length);
    bool exists(const QString &path) const;

    // Qt surface. One batched query per directory. Supported filter bits:
    // Files, Dirs, Hidden, Readable, Writable (others ignored). Supported
    // sort bits: Name, Time, Size, DirsFirst, DirsLast, Reversed, IgnoreCase.
    FileInfoList entryInfoList(const QString &path,
                               const QStringList &nameFilters = {},
                               QDir::Filters filters = QDir::NoFilter,
                               QDir::SortFlags sort = QDir::NoSort) const;

    // Convenience QIODevice over the same fds.
    std::unique_ptr<QIODevice> openDevice(const QString &path, QIODevice::OpenMode mode);

private:
    std::shared_ptr<Backend> d;
};

} // namespace StorageKit
