// SPDX-License-Identifier: MIT
#include "qmlfilesystem.h"

namespace StorageKit {

QmlFileSystem::QmlFileSystem(const FileSystem &fs, QObject *parent)
    : QObject(parent)
    , m_fs(fs)
{
}

} // namespace StorageKit
