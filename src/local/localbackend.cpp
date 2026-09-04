// SPDX-License-Identifier: MIT
#include "filesystem_p.h"

#include "fileinfo_p.h"

#include <QCoreApplication>
#include <QDirIterator>
#include <QFileInfo>
#include <QMimeDatabase>
#include <QPromise>

#ifdef STORAGEKIT_WIDGET_DIALOGS
#include <QFileDialog>
#endif

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

#ifdef STORAGEKIT_WIDGET_DIALOGS
// QFileDialog must run on the GUI thread; hop there and settle the promise
// from the dialog result.
template <typename T, typename Fn>
QFuture<T> runOnGuiThread(Fn dialogFn)
{
    auto promise = std::make_shared<QPromise<T>>();
    QFuture<T> future = promise->future();
    promise->start();
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        [promise, dialogFn]() {
            promise->addResult(dialogFn());
            promise->finish();
        },
        Qt::QueuedConnection);
    return future;
}

QStringList mimeFilterList(const QStringList &mimeTypes)
{
    return mimeTypes.isEmpty() ? QStringList{QStringLiteral("application/octet-stream")} : mimeTypes;
}
#endif

} // namespace

std::shared_ptr<Backend> createBackend(const QUrl &root)
{
    return std::make_shared<LocalBackend>(root);
}

#ifdef STORAGEKIT_WIDGET_DIALOGS

QFuture<FileSystem> platformPickFolder()
{
    return runOnGuiThread<FileSystem>([] {
        const QUrl url = QFileDialog::getExistingDirectoryUrl();
        return FileSystem(url);
    });
}

QFuture<QUrl> platformPickFile(const QStringList &mimeTypes)
{
    return runOnGuiThread<QUrl>([mimeTypes] {
        QFileDialog dialog;
        dialog.setFileMode(QFileDialog::ExistingFile);
        if (!mimeTypes.isEmpty())
            dialog.setMimeTypeFilters(mimeFilterList(mimeTypes));
        if (dialog.exec() != QDialog::Accepted || dialog.selectedUrls().isEmpty())
            return QUrl{};
        return dialog.selectedUrls().first();
    });
}

QFuture<QList<QUrl>> platformPickFiles(const QStringList &mimeTypes)
{
    return runOnGuiThread<QList<QUrl>>([mimeTypes] {
        QFileDialog dialog;
        dialog.setFileMode(QFileDialog::ExistingFiles);
        if (!mimeTypes.isEmpty())
            dialog.setMimeTypeFilters(mimeFilterList(mimeTypes));
        if (dialog.exec() != QDialog::Accepted)
            return QList<QUrl>{};
        return dialog.selectedUrls();
    });
}

QFuture<QUrl> platformPickSaveFile(const QString &suggestedName, const QString &mimeType)
{
    return runOnGuiThread<QUrl>([suggestedName, mimeType] {
        QFileDialog dialog;
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        if (!mimeType.isEmpty())
            dialog.setMimeTypeFilters({mimeType});
        if (!suggestedName.isEmpty())
            dialog.selectFile(suggestedName);
        if (dialog.exec() != QDialog::Accepted || dialog.selectedUrls().isEmpty())
            return QUrl{};
        // Create the file so the URL behaves like ACTION_CREATE_DOCUMENT's.
        const QUrl url = dialog.selectedUrls().first();
        QFile file(url.toLocalFile());
        if (!file.exists())
            file.open(QIODevice::WriteOnly);
        return url;
    });
}

#else // no widget dialogs: pickers are unavailable on this build

template <typename T>
static QFuture<T> unsupportedFuture()
{
    QPromise<T> promise;
    QFuture<T> future = promise.future();
    promise.start();
    promise.addResult(T{});
    promise.finish();
    return future;
}

QFuture<FileSystem> platformPickFolder()
{
    return unsupportedFuture<FileSystem>();
}

QFuture<QUrl> platformPickFile(const QStringList &)
{
    return unsupportedFuture<QUrl>();
}

QFuture<QList<QUrl>> platformPickFiles(const QStringList &)
{
    return unsupportedFuture<QList<QUrl>>();
}

QFuture<QUrl> platformPickSaveFile(const QString &, const QString &)
{
    return unsupportedFuture<QUrl>();
}

#endif

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
