// SPDX-License-Identifier: MIT
#include "filesystem_p.h"

#include "fileinfo_p.h"

#include <QFile>
#include <QRegularExpression>

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

#include <algorithm>

namespace StorageKit {

namespace {

// "" refers to the root. Returns false (and sets errno) for absolute paths,
// "..", "." or empty segments — the capability guarantee.
bool cleanPath(const QString &path, QString *out)
{
    if (path.isEmpty()) {
        out->clear();
        return true;
    }
    if (path.startsWith(u'/')) {
        errno = EACCES;
        return false;
    }
    const QStringList segments = path.split(u'/');
    for (const QString &s : segments) {
        if (s == u".." || s.contains(u'\\')) {
            errno = EACCES;
            return false;
        }
        if (s.isEmpty() || s == u".") {
            errno = EINVAL;
            return false;
        }
    }
    *out = segments.join(u'/');
    return true;
}

bool matchesFilters(const FileInfo &fi, QDir::Filters filters)
{
    if (filters == QDir::NoFilter)
        return true;
    if (fi.isDir() && !(filters & QDir::Dirs))
        return false;
    if (!fi.isDir() && !(filters & QDir::Files))
        return false;
    if (fi.fileName().startsWith(u'.') && !(filters & QDir::Hidden))
        return false;
    if ((filters & QDir::Readable) && !fi.isReadable())
        return false;
    if ((filters & QDir::Writable) && !fi.isWritable())
        return false;
    return true;
}

bool matchesNames(const FileInfo &fi, const QList<QRegularExpression> &patterns)
{
    if (patterns.isEmpty())
        return true;
    return std::any_of(patterns.cbegin(), patterns.cend(), [&](const QRegularExpression &re) {
        return re.match(fi.fileName()).hasMatch();
    });
}

void sortEntries(FileInfoList &list, QDir::SortFlags sort)
{
    if (sort == QDir::NoSort)
        return;
    const auto cs = (sort & QDir::IgnoreCase) ? Qt::CaseInsensitive : Qt::CaseSensitive;
    const auto key = sort & QDir::SortByMask;
    std::stable_sort(list.begin(), list.end(), [&](const FileInfo &a, const FileInfo &b) {
        if ((sort & QDir::DirsFirst) && a.isDir() != b.isDir())
            return a.isDir();
        if ((sort & QDir::DirsLast) && a.isDir() != b.isDir())
            return b.isDir();
        bool less;
        if (key == QDir::Time)
            less = a.lastModified() < b.lastModified();
        else if (key == QDir::Size)
            less = a.size() < b.size();
        else
            less = QString::compare(a.fileName(), b.fileName(), cs) < 0;
        return (sort & QDir::Reversed) ? !less : less;
    });
}

} // namespace

FileSystem::FileSystem()
    : FileSystem(QUrl{})
{
}

FileSystem::FileSystem(const QUrl &root)
    : d(createBackend(root))
{
}

bool FileSystem::isValid() const
{
    return d && d->isValid();
}

QUrl FileSystem::root() const
{
    return d ? d->root() : QUrl{};
}

QFuture<FileSystem> FileSystem::pickFolder()
{
    return platformPickFolder();
}

QFuture<QUrl> FileSystem::pickFile(const QStringList &mimeTypes)
{
    return platformPickFile(mimeTypes);
}

QFuture<QList<QUrl>> FileSystem::pickFiles(const QStringList &mimeTypes)
{
    return platformPickFiles(mimeTypes);
}

QFuture<QUrl> FileSystem::pickSaveFile(const QString &suggestedName, const QString &mimeType)
{
    return platformPickSaveFile(suggestedName, mimeType);
}

int FileSystem::openUrl(const QUrl &url, int flags)
{
    return platformOpenUrl(url, flags);
}

FileInfo FileSystem::urlInfo(const QUrl &url)
{
    return platformUrlInfo(url);
}

void FileSystem::releaseGrant(const QUrl &url)
{
    platformReleaseGrant(url);
}

int FileSystem::open(const QString &path, int flags)
{
    QString clean;
    if (!cleanPath(path, &clean))
        return -1;
    if (clean.isEmpty()) {
        errno = EISDIR;
        return -1;
    }
    return d->openFd(clean, flags);
}

int FileSystem::stat(const QString &path, Stat *out) const
{
    QString clean;
    if (!cleanPath(path, &clean))
        return -1;
    return d->statOne(clean, out);
}

int FileSystem::mkdir(const QString &path)
{
    QString clean;
    if (!cleanPath(path, &clean))
        return -1;
    if (clean.isEmpty()) {
        errno = EEXIST;
        return -1;
    }
    return d->mkdir(clean);
}

int FileSystem::rmdir(const QString &path)
{
    QString clean;
    if (!cleanPath(path, &clean))
        return -1;
    if (clean.isEmpty()) {
        errno = EACCES;
        return -1;
    }
    return d->rmdir(clean);
}

int FileSystem::unlink(const QString &path)
{
    QString clean;
    if (!cleanPath(path, &clean))
        return -1;
    if (clean.isEmpty()) {
        errno = EISDIR;
        return -1;
    }
    return d->unlink(clean);
}

int FileSystem::rename(const QString &from, const QString &to)
{
    QString cleanFrom, cleanTo;
    if (!cleanPath(from, &cleanFrom) || !cleanPath(to, &cleanTo))
        return -1;
    if (cleanFrom.isEmpty() || cleanTo.isEmpty()) {
        errno = EACCES;
        return -1;
    }
    return d->rename(cleanFrom, cleanTo);
}

int FileSystem::truncate(const QString &path, qint64 length)
{
    QString clean;
    if (!cleanPath(path, &clean))
        return -1;
    if (clean.isEmpty() || length < 0) {
        errno = EINVAL;
        return -1;
    }
    return d->truncate(clean, length);
}

bool FileSystem::exists(const QString &path) const
{
    QString clean;
    if (!cleanPath(path, &clean))
        return false;
    return d->exists(clean);
}

FileInfoList FileSystem::entryInfoList(const QString &path, const QStringList &nameFilters,
                                       QDir::Filters filters, QDir::SortFlags sort) const
{
    QString clean;
    if (!cleanPath(path, &clean))
        return {};

    FileInfoList raw;
    if (d->rawList(clean, &raw) != 0)
        return {};

    QList<QRegularExpression> patterns;
    patterns.reserve(nameFilters.size());
    for (const QString &glob : nameFilters)
        patterns.append(QRegularExpression::fromWildcard(glob, Qt::CaseInsensitive));

    FileInfoList result;
    for (const FileInfo &fi : std::as_const(raw)) {
        if (matchesFilters(fi, filters) && matchesNames(fi, patterns))
            result.append(fi);
    }
    sortEntries(result, sort);
    return result;
}

std::unique_ptr<QIODevice> FileSystem::openDevice(const QString &path, QIODevice::OpenMode mode)
{
    int flags = 0;
    const bool r = mode & QIODevice::ReadOnly;
    const bool w = mode & QIODevice::WriteOnly;
    if (r && w)
        flags = O_RDWR | O_CREAT;
    else if (w)
        flags = O_WRONLY | O_CREAT | ((mode & QIODevice::Append) ? 0 : O_TRUNC);
    else if (r)
        flags = O_RDONLY;
    else {
        errno = EINVAL;
        return nullptr;
    }
    if (mode & QIODevice::Append)
        flags |= O_APPEND;
    if (mode & QIODevice::Truncate)
        flags |= O_TRUNC;

    const int fd = open(path, flags);
    if (fd < 0)
        return nullptr;
    auto file = std::make_unique<QFile>();
    if (!file->open(fd, mode, QFileDevice::AutoCloseHandle)) {
        ::close(fd);
        errno = EIO;
        return nullptr;
    }
    return file;
}

} // namespace StorageKit
