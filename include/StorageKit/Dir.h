// SPDX-License-Identifier: MIT
#pragma once

#include <StorageKit/FileInfo.h>

#include <QDir>
#include <QUrl>

#include <memory>

namespace StorageKit {

class Backend;

/*!
 * A directory inside a user-granted root, mirroring QDir: a plain directory
 * (file:// URL) on desktop, a Storage Access Framework tree (content:// URL)
 * on Android. Instances cannot reach outside the granted root — cd("..")
 * at the root fails, absolute paths are rejected.
 *
 * Persist rootUrl() (e.g. in QSettings) and rebuild with the constructor to
 * keep access across runs; the folder picker takes a persistable URI
 * permission on Android. Like QDir, mutating calls return bool.
 *
 * Blocking API — call from a worker thread on Android (binder IPC).
 */
class Dir
{
public:
    explicit Dir(const QUrl &rootUrl);
    Dir(const Dir &other);
    Dir &operator=(const Dir &other);
    ~Dir();

    bool exists() const; // grant alive and current directory present
    QString dirName() const;
    QUrl rootUrl() const;
    QString path() const; // relative to the root, "" at the root

    bool cd(const QString &dirName);
    bool cdUp();

    // One batched query per call. Supported filter bits: Files, Dirs, Hidden,
    // Readable, Writable. Supported sort bits: Name, Time, Size, DirsFirst,
    // DirsLast, Reversed, IgnoreCase. Other bits are ignored.
    FileInfoList entryInfoList(const QStringList &nameFilters = {},
                               QDir::Filters filters = QDir::NoFilter,
                               QDir::SortFlags sort = QDir::NoSort) const;

    bool exists(const QString &name) const;
    bool mkdir(const QString &name);
    bool rmdir(const QString &name);
    bool remove(const QString &name);
    bool rename(const QString &from, const QString &to);

private:
    friend class File;
    QString filePath(const QString &name) const; // validated absolute-relative path

    std::shared_ptr<Backend> d;
    QString m_path;
};

} // namespace StorageKit
