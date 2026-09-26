// SPDX-License-Identifier: MIT
#include "storage.h"

#include <StorageKit/Dir.h>
#include <StorageKit/File.h>
#include <StorageKit/FileDialog.h>

#include <QtConcurrent>

namespace StorageKit {

Storage::Storage(QObject *parent)
    : QObject(parent)
{
}

void Storage::pickFolder()
{
    FileDialog::getExistingDirectoryUrl().then(this, [this](const QUrl &url) {
        if (url.isValid())
            emit folderPicked(url);
        else
            emit pickCanceled();
    });
}

void Storage::pickFile(const QStringList &mimeTypes)
{
    FileDialog::getOpenFileUrl(mimeTypes).then(this, [this](const QUrl &url) {
        if (url.isValid())
            emit filePicked(url);
        else
            emit pickCanceled();
    });
}

void Storage::pickFiles(const QStringList &mimeTypes)
{
    FileDialog::getOpenFileUrls(mimeTypes).then(this, [this](const QList<QUrl> &urls) {
        if (!urls.isEmpty())
            emit filesPicked(urls);
        else
            emit pickCanceled();
    });
}

void Storage::pickSaveFile(const QString &suggestedName, const QString &mimeType)
{
    FileDialog::getSaveFileUrl(suggestedName, mimeType).then(this, [this](const QUrl &url) {
        if (url.isValid())
            emit saveFilePicked(url);
        else
            emit pickCanceled();
    });
}

bool Storage::isGrantValid(const QUrl &rootUrl) const
{
    return rootUrl.isValid() && Dir(rootUrl).exists();
}

void Storage::readAll(const QUrl &rootUrl, const QString &path)
{
    auto future = QtConcurrent::run([rootUrl, path] {
        File file(Dir(rootUrl), path);
        if (!file.open(QIODevice::ReadOnly))
            return std::make_pair(false, QByteArray{});
        return std::make_pair(true, file.readAll());
    });
    future.then(this, [this, path](const std::pair<bool, QByteArray> &result) {
        emit fileRead(path, QString::fromUtf8(result.second), result.first);
    });
}

void Storage::writeAll(const QUrl &rootUrl, const QString &path, const QString &text)
{
    const QByteArray content = text.toUtf8();
    auto future = QtConcurrent::run([rootUrl, content, path] {
        File file(Dir(rootUrl), path);
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate))
            return false;
        return file.write(content) == content.size();
    });
    future.then(this, [this, path](bool ok) { emit fileWritten(path, ok); });
}

} // namespace StorageKit
