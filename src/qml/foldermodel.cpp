// SPDX-License-Identifier: MIT
#include "foldermodel.h"

#include <QtConcurrent>

#include <cerrno>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>

namespace StorageKit {

FolderModel::FolderModel(QObject *parent)
    : QAbstractListModel(parent)
{
}

void FolderModel::setFileSystem(QmlFileSystem *fs)
{
    if (m_fileSystem == fs)
        return;
    m_fileSystem = fs;
    m_path.clear();
    emit fileSystemChanged();
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
    if (!m_fileSystem || !m_fileSystem->valid()) {
        beginResetModel();
        m_entries.clear();
        endResetModel();
        return;
    }
    const quint64 generation = ++m_generation;
    const FileSystem fs = m_fileSystem->fileSystem();
    const QString path = m_path;
    setBusy(true);
    auto future = QtConcurrent::run([fs, path]() mutable {
        return fs.entryInfoList(path, {}, QDir::NoFilter,
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
    const QString path = childPath(name);
    runMutation([path](FileSystem &fs) {
        const int fd = fs.open(path, O_WRONLY | O_CREAT | O_EXCL);
        if (fd < 0)
            return -1;
        ::close(fd);
        return 0;
    });
}

void FolderModel::newFolder(const QString &name)
{
    const QString path = childPath(name);
    runMutation([path](FileSystem &fs) { return fs.mkdir(path); });
}

void FolderModel::renameItem(int index, const QString &newName)
{
    if (!validIndex(index) || newName.isEmpty())
        return;
    const QString from = m_entries[index].filePath();
    const QString to = childPath(newName);
    runMutation([from, to](FileSystem &fs) { return fs.rename(from, to); });
}

void FolderModel::removeItem(int index)
{
    if (!validIndex(index))
        return;
    const QString path = m_entries[index].filePath();
    const bool dir = m_entries[index].isDir();
    runMutation([path, dir](FileSystem &fs) { return dir ? fs.rmdir(path) : fs.unlink(path); });
}

void FolderModel::runMutation(std::function<int(FileSystem &)> op)
{
    if (!m_fileSystem || !m_fileSystem->valid())
        return;
    FileSystem fs = m_fileSystem->fileSystem();
    setBusy(true);
    auto future = QtConcurrent::run([fs, op = std::move(op)]() mutable {
        return op(fs) == 0 ? 0 : errno;
    });
    future.then(this, [this](int error) {
        if (error != 0)
            emit errorOccurred(QString::fromLocal8Bit(std::strerror(error)));
        refresh();
    });
}

QString FolderModel::childPath(const QString &name) const
{
    return m_path.isEmpty() ? name : m_path + u'/' + name;
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
