// SPDX-License-Identifier: MIT
#pragma once

#include "qmlfilesystem.h"

#include <QObject>
#include <QQmlEngine>

namespace StorageKit {

/*!
 * QML entry point: pickers and small-file conveniences. All storage work
 * runs off the GUI thread; results come back through signals.
 */
class StorageSingleton : public QObject
{
    Q_OBJECT
    QML_NAMED_ELEMENT(Storage)
    QML_SINGLETON

public:
    explicit StorageSingleton(QObject *parent = nullptr);

    Q_INVOKABLE void pickFolder();
    Q_INVOKABLE void pickFile(const QStringList &mimeTypes = {});
    Q_INVOKABLE void pickFiles(const QStringList &mimeTypes = {});
    Q_INVOKABLE StorageKit::QmlFileSystem *restore(const QUrl &root);

    // Small files only (fully buffered). Results arrive via fileRead/fileWritten.
    Q_INVOKABLE void readAll(StorageKit::QmlFileSystem *fs, const QString &path);
    Q_INVOKABLE void writeAll(StorageKit::QmlFileSystem *fs, const QString &path, const QString &text);

signals:
    void folderPicked(StorageKit::QmlFileSystem *fileSystem);
    void filePicked(const QUrl &url);
    void filesPicked(const QList<QUrl> &urls);
    void pickCanceled();
    void fileRead(const QString &path, const QString &text, bool ok);
    void fileWritten(const QString &path, bool ok);
};

} // namespace StorageKit
