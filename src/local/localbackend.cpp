// SPDX-License-Identifier: MIT
#include "filesystem_p.h"

#include "fileinfo_p.h"

#include <QDirIterator>
#include <QFileInfo>
#include <QMimeDatabase>

#include <cerrno>
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace StorageKit {

namespace {

QString mimeForPath(const QString &absPath, bool isDir)
{
    if (isDir)
        return QStringLiteral("inode/directory");
    static thread_local QMimeDatabase db;
    return db.mimeTypeForFile(absPath, QMimeDatabase::MatchExtension).name();
}

FileInfo makeInfo(const QFileInfo &qfi, const QString &relPath)
{
    auto *data = new FileInfoData;
    data->name = qfi.fileName();
    data->filePath = relPath;
    data->size = qfi.isDir() ? 0 : qfi.size();
    data->dir = qfi.isDir();
    data->present = qfi.exists();
    data->mtime = qfi.lastModified();
    data->readable = qfi.isReadable();
    data->writable = qfi.isWritable();
    data->mime = mimeForPath(qfi.absoluteFilePath(), qfi.isDir());
    return FileInfo(data);
}

class LocalBackend : public Backend
{
public:
    explicit LocalBackend(const QUrl &root)
        : m_root(root)
        , m_rootDir(root.isLocalFile() ? root.toLocalFile() : QString{})
    {
    }

    bool isValid() const override
    {
        if (m_rootDir.isEmpty())
            return false;
        const QFileInfo fi(m_rootDir);
        return fi.isDir() && fi.isReadable();
    }

    QUrl root() const override { return m_root; }

    int openFd(const QString &path, int flags) override
    {
        return ::open(abs(path).toLocal8Bit().constData(), flags, 0644);
    }

    int statOne(const QString &path, Stat *out) override
    {
        const QFileInfo qfi(abs(path));
        if (!qfi.exists()) {
            errno = ENOENT;
            return -1;
        }
        if (out) {
            out->type = qfi.isDir() ? Stat::Dir : Stat::File;
            out->size = qfi.isDir() ? 0 : qfi.size();
            out->mtime = qfi.lastModified();
            out->mime = mimeForPath(qfi.absoluteFilePath(), qfi.isDir());
            out->readable = qfi.isReadable();
            out->writable = qfi.isWritable();
        }
        return 0;
    }

    int mkdir(const QString &path) override
    {
        return ::mkdir(abs(path).toLocal8Bit().constData(), 0755);
    }

    int rmdir(const QString &path) override
    {
        return ::rmdir(abs(path).toLocal8Bit().constData());
    }

    int unlink(const QString &path) override
    {
        return ::unlink(abs(path).toLocal8Bit().constData());
    }

    int rename(const QString &from, const QString &to) override
    {
        return ::rename(abs(from).toLocal8Bit().constData(), abs(to).toLocal8Bit().constData());
    }

    int truncate(const QString &path, qint64 length) override
    {
        return ::truncate(abs(path).toLocal8Bit().constData(), length);
    }

    bool exists(const QString &path) override
    {
        return QFileInfo::exists(abs(path));
    }

    int rawList(const QString &path, FileInfoList *out) override
    {
        const QString dirPath = abs(path);
        const QFileInfo dirInfo(dirPath);
        if (!dirInfo.exists()) {
            errno = ENOENT;
            return -1;
        }
        if (!dirInfo.isDir()) {
            errno = ENOTDIR;
            return -1;
        }
        QDirIterator it(dirPath, QDir::AllEntries | QDir::NoDotAndDotDot | QDir::Hidden);
        while (it.hasNext()) {
            it.next();
            const QString rel = path.isEmpty() ? it.fileName() : path + u'/' + it.fileName();
            out->append(makeInfo(it.fileInfo(), rel));
        }
        return 0;
    }

private:
    QString abs(const QString &path) const
    {
        return path.isEmpty() ? m_rootDir : m_rootDir + u'/' + path;
    }

    QUrl m_root;
    QString m_rootDir;
};

} // namespace

std::shared_ptr<Backend> createBackend(const QUrl &root)
{
    return std::make_shared<LocalBackend>(root);
}

int platformOpenUrl(const QUrl &url, int flags)
{
    if (!url.isLocalFile()) {
        errno = EINVAL;
        return -1;
    }
    return ::open(url.toLocalFile().toLocal8Bit().constData(), flags, 0644);
}

FileInfo platformUrlInfo(const QUrl &url)
{
    // Accept file:// URLs and plain local paths.
    const QString path = url.isLocalFile() ? url.toLocalFile() : url.toString();
    const QFileInfo qfi(path);
    if (path.isEmpty() || !qfi.exists())
        return {};
    return makeInfo(qfi, qfi.absoluteFilePath());
}

void platformReleaseGrant(const QUrl &)
{
    // Desktop paths carry no revocable grant.
}

} // namespace StorageKit
