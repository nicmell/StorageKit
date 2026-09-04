// SPDX-License-Identifier: MIT
#pragma once

#include <StorageKit/FileInfo.h>

#include <QSharedData>

namespace StorageKit {

class FileInfoData : public QSharedData
{
public:
    QString name;
    QString filePath;
    qint64 size = 0;
    bool dir = false;
    bool present = false;
    QDateTime mtime;
    bool readable = false;
    bool writable = false;
    QString mime;
};

} // namespace StorageKit
