// SPDX-License-Identifier: MIT
#pragma once

#include <StorageKit/FileSystem.h>

#include <QObject>
#include <QQmlEngine>

namespace StorageKit {

/*!
 * QObject handle over a FileSystem, so QML can hold and pass granted roots.
 * Created by Storage.pickFolder()/Storage.restore(), not instantiable from QML.
 */
class QmlFileSystem : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(FileSystem)
    QML_UNCREATABLE("Obtained from Storage.pickFolder() or Storage.restore()")
    Q_PROPERTY(QUrl root READ root CONSTANT)
    Q_PROPERTY(bool valid READ valid CONSTANT)

public:
    explicit QmlFileSystem(const FileSystem &fs, QObject *parent = nullptr);

    QUrl root() const { return m_fs.root(); }
    bool valid() const { return m_fs.isValid(); }
    const FileSystem &fileSystem() const { return m_fs; }

private:
    FileSystem m_fs;
};

} // namespace StorageKit
