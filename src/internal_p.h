// SPDX-License-Identifier: MIT
#pragma once

#include "filesystem_p.h"

#include <QRegularExpression>

#include <fcntl.h>

#include <algorithm>

// Shared helpers for the public classes (Dir, File): relative-path
// validation, entry filtering/sorting, OpenMode mapping.
namespace StorageKit {
namespace internal {

// "" refers to the current/root directory. Rejects absolute paths, "..",
// "." and empty segments — the capability guarantee.
inline bool cleanPath(const QString &path, QString *out)
{
    if (path.isEmpty()) {
        out->clear();
        return true;
    }
    if (path.startsWith(u'/'))
        return false;
    const QStringList segments = path.split(u'/');
    for (const QString &s : segments) {
        if (s.isEmpty() || s == u"." || s == u".." || s.contains(u'\\'))
            return false;
    }
    *out = segments.join(u'/');
    return true;
}

inline QString joinPath(const QString &base, const QString &name)
{
    return base.isEmpty() ? name : base + u'/' + name;
}

inline int posixFlags(QIODevice::OpenMode mode)
{
    int flags = 0;
    const bool r = mode & QIODevice::ReadOnly;
    const bool w = mode & QIODevice::WriteOnly;
    if (r && w)
        flags = O_RDWR | O_CREAT;
    else if (w)
        flags = O_WRONLY | O_CREAT | ((mode & QIODevice::Append) ? 0 : O_TRUNC);
    else
        flags = O_RDONLY;
    if (mode & QIODevice::Append)
        flags |= O_APPEND;
    if (mode & QIODevice::Truncate)
        flags |= O_TRUNC;
    return flags;
}

inline bool matchesFilters(const FileInfo &fi, QDir::Filters filters)
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

inline void sortEntries(FileInfoList &list, QDir::SortFlags sort)
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

inline FileInfoList filterAndSort(const FileInfoList &raw, const QStringList &nameFilters,
                                  QDir::Filters filters, QDir::SortFlags sort)
{
    QList<QRegularExpression> patterns;
    patterns.reserve(nameFilters.size());
    for (const QString &glob : nameFilters)
        patterns.append(QRegularExpression::fromWildcard(glob, Qt::CaseInsensitive));

    FileInfoList result;
    for (const FileInfo &fi : raw) {
        const bool nameOk = patterns.isEmpty()
            || std::any_of(patterns.cbegin(), patterns.cend(), [&](const QRegularExpression &re) {
                   return re.match(fi.fileName()).hasMatch();
               });
        if (nameOk && matchesFilters(fi, filters))
            result.append(fi);
    }
    sortEntries(result, sort);
    return result;
}

} // namespace internal
} // namespace StorageKit
