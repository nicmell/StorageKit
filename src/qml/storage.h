// SPDX-License-Identifier: MIT
#pragma once

#include <StorageKit/FileSystem.h>

#include <QObject>
#include <QQmlEngine>

namespace StorageKit {

/*!
 * QML entry point: pickers and small-file conveniences. All storage work
 * runs off the GUI thread; results come back through signals.
 */
class Storage : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_SINGLETON

public:
    explicit Storage(QObject *parent = nullptr);

    Q_INVOKABLE void pickFolder();
    Q_INVOKABLE void pickFile(const QStringList &mimeTypes = {});
    Q_INVOKABLE void pickFiles(const QStringList &mimeTypes = {});
    Q_INVOKABLE void pickSaveFile(const QString &suggestedName, const QString &mimeType = {});
    Q_INVOKABLE StorageKit::FileSystem *restore(const QUrl &root);

    // Small files only (fully buffered). Results arrive via fileRead/fileWritten.
    Q_INVOKABLE void readAll(StorageKit::FileSystem *fs, const QString &path);
    Q_INVOKABLE void writeAll(StorageKit::FileSystem *fs, const QString &path, const QString &text);

signals:
    void folderPicked(const QUrl &url);
    void filePicked(const QUrl &url);
    void filesPicked(const QList<QUrl> &urls);
    void saveFilePicked(const QUrl &url);
    void pickCanceled();
    void fileRead(const QString &path, const QString &text, bool ok);
    void fileWritten(const QString &path, bool ok);
};

} // namespace StorageKit
