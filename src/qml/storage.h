// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QQmlEngine>
#include <QUrl>

namespace StorageKit {

/*!
 * QML entry point: pickers and small-file conveniences. All storage work
 * runs off the GUI thread; results come back through signals. Granted roots
 * are plain urls — persist them and hand them to FolderModel.rootUrl.
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

    // The grant behind a persisted root url is still alive and reachable.
    Q_INVOKABLE bool isGrantValid(const QUrl &rootUrl) const;

    // Small files only (fully buffered). Results arrive via fileRead/fileWritten.
    Q_INVOKABLE void readAll(const QUrl &rootUrl, const QString &path);
    Q_INVOKABLE void writeAll(const QUrl &rootUrl, const QString &path, const QString &text);

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
