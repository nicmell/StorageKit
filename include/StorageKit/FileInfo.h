// SPDX-License-Identifier: MIT
#pragma once

#include <QDateTime>
#include <QList>
#include <QSharedDataPointer>
#include <QString>

namespace StorageKit {

class FileInfoData;

/*!
 * Metadata for one entry inside a FileSystem, mirroring a subset of QFileInfo
 * plus mimeType(). Instances are produced by FileSystem::entryInfoList() and
 * filled from a single batched query per directory.
 */
class FileInfo
{
public:
    FileInfo();
    FileInfo(const FileInfo &other);
    FileInfo &operator=(const FileInfo &other);
    ~FileInfo();

    QString fileName() const;
    QString filePath() const; // relative to the owning FileSystem root
    qint64 size() const;
    bool isDir() const;
    bool isFile() const;
    bool exists() const;
    QDateTime lastModified() const;
    bool isReadable() const;
    bool isWritable() const;
    QString suffix() const;
    QString baseName() const;
    QString mimeType() const; // extension over QFileInfo, free on Android (SAF column)

    // Internal: used by backends to construct instances.
    explicit FileInfo(FileInfoData *data);

private:
    QSharedDataPointer<FileInfoData> d;
};

using FileInfoList = QList<FileInfo>;

} // namespace StorageKit
