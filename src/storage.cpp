// SPDX-License-Identifier: MIT
#include "storage.h"

#include <QtConcurrent>

#include <fcntl.h>
#include <unistd.h>

namespace StorageKit {

Storage::Storage(QObject *parent)
    : QObject(parent)
{
}

void Storage::pickFolder()
{
    FileSystem::pickFolder().then(this, [this](const QUrl &url) {
        if (url.isValid())
            emit folderPicked(url);
        else
            emit pickCanceled();
    });
}

void Storage::pickFile(const QStringList &mimeTypes)
{
    FileSystem::pickFile(mimeTypes).then(this, [this](const QUrl &url) {
        if (url.isValid())
            emit filePicked(url);
        else
            emit pickCanceled();
    });
}

void Storage::pickFiles(const QStringList &mimeTypes)
{
    FileSystem::pickFiles(mimeTypes).then(this, [this](const QList<QUrl> &urls) {
        if (!urls.isEmpty())
            emit filesPicked(urls);
        else
            emit pickCanceled();
    });
}

void Storage::pickSaveFile(const QString &suggestedName, const QString &mimeType)
{
    FileSystem::pickSaveFile(suggestedName, mimeType).then(this, [this](const QUrl &url) {
        if (url.isValid())
            emit saveFilePicked(url);
        else
            emit pickCanceled();
    });
}

FileSystem *Storage::restore(const QUrl &root)
{
    return new FileSystem(root, this);
}

void Storage::readAll(FileSystem *fs, const QString &path)
{
    if (!fs) {
        emit fileRead(path, {}, false);
        return;
    }
    const QUrl root = fs->root();
    auto future = QtConcurrent::run([root, path] {
        FileSystem fileSystem(root);
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

void Storage::writeAll(FileSystem *fs, const QString &path, const QString &text)
{
    if (!fs) {
        emit fileWritten(path, false);
        return;
    }
    const QUrl root = fs->root();
    const QByteArray content = text.toUtf8();
    auto future = QtConcurrent::run([root, content, path] {
        FileSystem fileSystem(root);
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
