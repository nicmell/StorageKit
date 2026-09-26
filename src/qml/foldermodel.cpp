// SPDX-License-Identifier: MIT
#include "foldermodel.h"

#include <StorageKit/File.h>

#include <QtConcurrent>

namespace StorageKit {

namespace {

// Enters `path` from the root; false when a segment is missing.
bool descend(Dir &dir, const QString &path)
{
    return path.isEmpty() || dir.cd(path);
}

} // namespace

FolderModel::FolderModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void FolderModel::setRootUrl(const QUrl &rootUrl)
{
    if (m_rootUrl == rootUrl)
        return;
    m_rootUrl = rootUrl;
    m_path.clear();
    emit rootUrlChanged();
    emit pathChanged();
    refresh();
}

void FolderModel::setPath(const QString &path)
{
    if (m_path == path)
        return;
    m_path = path;
    emit pathChanged();
    refresh();
}

void FolderModel::refresh()
{
    if (!m_rootUrl.isValid()) {
        beginResetModel();
        m_entries.clear();
        endResetModel();
        return;
    }
    const quint64 generation = ++m_generation;
    const QUrl root = m_rootUrl;
    const QString path = m_path;
    setBusy(true);
    auto future = QtConcurrent::run([root, path] {
        Dir dir(root);
        if (!descend(dir, path))
            return FileInfoList{};
        return dir.entryInfoList({}, QDir::NoFilter,
                                 QDir::Name | QDir::DirsFirst | QDir::IgnoreCase);
    });
    future.then(this, [this, generation](const FileInfoList &entries) {
        if (generation != m_generation)
            return; // a newer refresh superseded this one
        beginResetModel();
        m_entries = entries;
        endResetModel();
        setBusy(false);
    });
}

void FolderModel::openItem(int index)
{
    if (validIndex(index) && m_entries[index].isDir())
        setPath(m_entries[index].filePath());
}

void FolderModel::cdUp()
{
    const int slash = m_path.lastIndexOf(u'/');
    setPath(slash < 0 ? QString{} : m_path.left(slash));
}

QString FolderModel::itemPath(int index) const
{
    return validIndex(index) ? m_entries[index].filePath() : QString{};
}

bool FolderModel::itemIsDir(int index) const
{
    return validIndex(index) && m_entries[index].isDir();
}

void FolderModel::newFile(const QString &name)
{
    runMutation(tr("create %1").arg(name), [name](Dir &dir) {
        File file(dir, name);
        if (file.exists())
            return false;
        return file.open(QIODevice::WriteOnly);
    });
}

void FolderModel::newFolder(const QString &name)
{
    runMutation(tr("create %1").arg(name), [name](Dir &dir) { return dir.mkdir(name); });
}

void FolderModel::renameItem(int index, const QString &newName)
{
    if (!validIndex(index) || newName.isEmpty())
        return;
    const QString name = m_entries[index].fileName();
    runMutation(tr("rename %1").arg(name),
                [name, newName](Dir &dir) { return dir.rename(name, newName); });
}

void FolderModel::removeItem(int index)
{
    if (!validIndex(index))
        return;
    const QString name = m_entries[index].fileName();
    const bool isDir = m_entries[index].isDir();
    runMutation(tr("remove %1").arg(name),
                [name, isDir](Dir &dir) { return isDir ? dir.rmdir(name) : dir.remove(name); });
}

// Runs `op` against the model's current directory on a worker thread.
void FolderModel::runMutation(const QString &description, std::function<bool(Dir &)> op)
{
    if (!m_rootUrl.isValid())
        return;
    const QUrl root = m_rootUrl;
    const QString path = m_path;
    setBusy(true);
    auto future = QtConcurrent::run([root, path, op = std::move(op)] {
        Dir dir(root);
        return descend(dir, path) && op(dir);
    });
    future.then(this, [this, description](bool ok) {
        if (!ok)
            emit errorOccurred(tr("Operation failed: %1").arg(description));
        refresh();
    });
}

void FolderModel::setBusy(bool busy)
{
    if (m_busy == busy)
        return;
    m_busy = busy;
    emit busyChanged();
}

int FolderModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_entries.size();
}

QVariant FolderModel::data(const QModelIndex &index, int role) const
{
    if (!validIndex(index.row()))
        return {};
    const FileInfo &fi = m_entries[index.row()];
    switch (role) {
    case NameRole:
        return fi.fileName();
    case TypeRole:
        return fi.isDir() ? QStringLiteral("dir") : QStringLiteral("file");
    case SizeRole:
        return double(fi.size());
    case MimeRole:
        return fi.mimeType();
    }
    return {};
}

QHash<int, QByteArray> FolderModel::roleNames() const
{
    return {{NameRole, "name"}, {TypeRole, "type"}, {SizeRole, "size"}, {MimeRole, "mime"}};
}

} // namespace StorageKit
