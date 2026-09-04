// SPDX-License-Identifier: MIT
#include "fileinfo_p.h"

namespace StorageKit {

FileInfo::FileInfo()
    : d(new FileInfoData)
{
}

FileInfo::FileInfo(FileInfoData *data)
    : d(data)
{
}

FileInfo::FileInfo(const FileInfo &other) = default;
FileInfo &FileInfo::operator=(const FileInfo &other) = default;
FileInfo::~FileInfo() = default;

QString FileInfo::fileName() const
{
    return d->name;
}

QString FileInfo::filePath() const
{
    return d->filePath;
}

qint64 FileInfo::size() const
{
    return d->size;
}

bool FileInfo::isDir() const
{
    return d->dir;
}

bool FileInfo::isFile() const
{
    return d->present && !d->dir;
}

bool FileInfo::exists() const
{
    return d->present;
}

QDateTime FileInfo::lastModified() const
{
    return d->mtime;
}

bool FileInfo::isReadable() const
{
    return d->readable;
}

bool FileInfo::isWritable() const
{
    return d->writable;
}

QString FileInfo::suffix() const
{
    const int dot = d->name.lastIndexOf(u'.');
    return dot > 0 ? d->name.mid(dot + 1) : QString{};
}

QString FileInfo::baseName() const
{
    const int dot = d->name.indexOf(u'.', 1);
    return dot > 0 ? d->name.left(dot) : d->name;
}

QString FileInfo::mimeType() const
{
    return d->mime;
}

} // namespace StorageKit
