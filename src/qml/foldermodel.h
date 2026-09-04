// SPDX-License-Identifier: MIT
#pragma once

#include "qmlfilesystem.h"

#include <QAbstractListModel>
#include <QQmlEngine>

#include <functional>

namespace StorageKit {

/*!
 * List model over one directory of a FileSystem. Listing and mutations run
 * on a worker thread; the model resets back on the GUI thread when done.
 */
class FolderModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(StorageKit::QmlFileSystem *fileSystem READ fileSystem WRITE setFileSystem NOTIFY fileSystemChanged)
    Q_PROPERTY(QString path READ path WRITE setPath NOTIFY pathChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY busyChanged)
    Q_PROPERTY(bool canGoUp READ canGoUp NOTIFY pathChanged)

public:
    enum Roles { NameRole = Qt::UserRole + 1, TypeRole, SizeRole, MimeRole };

    explicit FolderModel(QObject *parent = nullptr);

    QmlFileSystem *fileSystem() const { return m_fileSystem; }
    void setFileSystem(QmlFileSystem *fs);
    QString path() const { return m_path; }
    void setPath(const QString &path);
    bool busy() const { return m_busy; }
    bool canGoUp() const { return !m_path.isEmpty(); }

    Q_INVOKABLE void refresh();
    Q_INVOKABLE void openItem(int index); // descends when the item is a directory
    Q_INVOKABLE void cdUp();
    Q_INVOKABLE QString itemPath(int index) const;
    Q_INVOKABLE bool itemIsDir(int index) const;
    Q_INVOKABLE void newFile(const QString &name);
    Q_INVOKABLE void newFolder(const QString &name);
    Q_INVOKABLE void renameItem(int index, const QString &newName);
    Q_INVOKABLE void removeItem(int index);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

signals:
    void fileSystemChanged();
    void pathChanged();
    void busyChanged();
    void errorOccurred(const QString &message);

private:
    void setBusy(bool busy);
    void runMutation(std::function<int(FileSystem &)> op);
    bool validIndex(int index) const { return index >= 0 && index < m_entries.size(); }
    QString childPath(const QString &name) const;

    QmlFileSystem *m_fileSystem = nullptr;
    QString m_path;
    FileInfoList m_entries;
    bool m_busy = false;
    quint64 m_generation = 0; // drops stale async listings
};

} // namespace StorageKit
