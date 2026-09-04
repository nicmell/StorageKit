// SPDX-License-Identifier: MIT
#include "storagesingleton.h"

#include <QtConcurrent>

#include <fcntl.h>
#include <unistd.h>

namespace StorageKit {

StorageSingleton::StorageSingleton(QObject *parent)
    : QObject(parent)
{
}

void StorageSingleton::pickFolder()
{
    FileSystem::pickFolder().then(this, [this](const FileSystem &fs) {
        if (fs.isValid())
            emit folderPicked(new QmlFileSystem(fs, this));
        else
            emit pickCanceled();
    });
}

void StorageSingleton::pickFile(const QStringList &mimeTypes)
{
    FileSystem::pickFile(mimeTypes).then(this, [this](const QUrl &url) {
        if (url.isValid())
            emit filePicked(url);
        else
            emit pickCanceled();
    });
}

void StorageSingleton::pickFiles(const QStringList &mimeTypes)
{
    FileSystem::pickFiles(mimeTypes).then(this, [this](const QList<QUrl> &urls) {
        if (!urls.isEmpty())
            emit filesPicked(urls);
        else
            emit pickCanceled();
    });
}

QmlFileSystem *StorageSingleton::restore(const QUrl &root)
{
    return new QmlFileSystem(FileSystem(root), this);
}

void StorageSingleton::readAll(QmlFileSystem *fs, const QString &path)
{
    if (!fs) {
        emit fileRead(path, {}, false);
        return;
    }
    FileSystem fileSystem = fs->fileSystem();
    auto future = QtConcurrent::run([fileSystem, path]() mutable {
        QByteArray content;
        const int fd = fileSystem.open(path, O_RDONLY);
        if (fd < 0)
            return std::make_pair(false, content);
        char buffer[16384];
        ssize_t n;
        while ((n = ::read(fd, buffer, sizeof(buffer))) > 0)
            content.append(buffer, n);
        ::close(fd);
        return std::make_pair(n == 0, content);
    });
    future.then(this, [this, path](const std::pair<bool, QByteArray> &result) {
        emit fileRead(path, QString::fromUtf8(result.second), result.first);
    });
}

void StorageSingleton::writeAll(QmlFileSystem *fs, const QString &path, const QString &text)
{
    if (!fs) {
        emit fileWritten(path, false);
        return;
    }
    FileSystem fileSystem = fs->fileSystem();
    const QByteArray content = text.toUtf8();
    auto future = QtConcurrent::run([fileSystem, content, path]() mutable {
        const int fd = fileSystem.open(path, O_WRONLY | O_CREAT | O_TRUNC);
        if (fd < 0)
            return false;
        qint64 written = 0;
        while (written < content.size()) {
            const ssize_t n = ::write(fd, content.constData() + written, content.size() - written);
            if (n <= 0) {
                ::close(fd);
                return false;
            }
            written += n;
        }
        ::close(fd);
        return true;
    });
    future.then(this, [this, path](bool ok) { emit fileWritten(path, ok); });
}

} // namespace StorageKit
