// SPDX-License-Identifier: MIT
#include <StorageKit/File.h>

#include "fileinfo_p.h"
#include "internal_p.h"

#include <QFile>

#include <cerrno>
#include <cstring>
#include <unistd.h>

namespace StorageKit {

class File::Private
{
public:
    // Tree-addressed: backend + relative path. Standalone: url only.
    std::shared_ptr<Backend> backend;
    QString path;      // null when the Dir-relative name failed validation
    QUrl url;
    QFile impl;        // adopts the native fd while open

    bool addressable() const { return backend ? !path.isNull() : url.isValid(); }

    QString lastSegment() const
    {
        const int slash = path.lastIndexOf(u'/');
        return slash < 0 ? path : path.mid(slash + 1);
    }

    int openFd(int flags)
    {
        return backend ? backend->openFd(path, flags) : StorageKit::openUrl(url, flags);
    }
};

File::File(const QUrl &url, QObject *parent)
    : QIODevice(parent)
    , d(std::make_unique<Private>())
{
    d->url = url;
}

File::File(const Dir &dir, const QString &fileName, QObject *parent)
    : QIODevice(parent)
    , d(std::make_unique<Private>())
{
    d->backend = dir.d;
    d->path = dir.filePath(fileName);
}

File::~File()
{
    if (isOpen())
        File::close();
}

bool File::open(QIODevice::OpenMode mode)
{
    if (isOpen()) {
        setErrorString(QStringLiteral("Device already open"));
        return false;
    }
    if (mode & QIODevice::Append)
        mode |= QIODevice::WriteOnly; // QFile convention
    if (!(mode & QIODevice::ReadWrite)) {
        setErrorString(QStringLiteral("Invalid open mode"));
        return false;
    }
    if (!d->addressable()) {
        setErrorString(QStringLiteral("Invalid file reference"));
        return false;
    }
    const int fd = d->openFd(internal::posixFlags(mode));
    if (fd < 0) {
        setErrorString(QString::fromLocal8Bit(std::strerror(errno)));
        return false;
    }
    if (!d->impl.open(fd, mode, QFileDevice::AutoCloseHandle)) {
        ::close(fd);
        setErrorString(d->impl.errorString());
        return false;
    }
    return QIODevice::open(mode);
}

void File::close()
{
    d->impl.close();
    QIODevice::close();
}

qint64 File::size() const
{
    return isOpen() ? d->impl.size() : fileInfo().size();
}

bool File::isSequential() const
{
    return false;
}

bool File::exists() const
{
    if (!d->addressable())
        return false;
    return d->backend ? d->backend->exists(d->path) : urlInfo(d->url).exists();
}

FileInfo File::fileInfo() const
{
    if (!d->addressable())
        return {};
    if (!d->backend)
        return urlInfo(d->url);
    Stat st;
    if (d->backend->statOne(d->path, &st) != 0)
        return {};
    auto *data = new FileInfoData;
    data->name = d->lastSegment();
    data->filePath = d->path;
    data->size = st.type == Stat::Dir ? 0 : st.size;
    data->dir = st.type == Stat::Dir;
    data->present = true;
    data->mtime = st.mtime;
    data->readable = st.readable;
    data->writable = st.writable;
    data->mime = st.mime;
    return FileInfo(data);
}

QString File::fileName() const
{
    if (d->backend)
        return d->lastSegment();
    return urlInfo(d->url).fileName();
}

QUrl File::url() const
{
    return d->url;
}

bool File::remove()
{
    if (isOpen() || !d->addressable())
        return false;
    return d->backend ? d->backend->unlink(d->path) == 0 : removeUrl(d->url);
}

bool File::rename(const QString &newName)
{
    if (isOpen() || !d->addressable() || newName.isEmpty() || newName.contains(u'/'))
        return false;
    if (d->backend) {
        const int slash = d->path.lastIndexOf(u'/');
        const QString newPath = slash < 0 ? newName : d->path.left(slash + 1) + newName;
        if (d->backend->rename(d->path, newPath) != 0)
            return false;
        d->path = newPath;
        return true;
    }
    const QUrl renamed = renameUrl(d->url, newName);
    if (renamed.isEmpty())
        return false;
    d->url = renamed;
    return true;
}

bool File::resize(qint64 size)
{
    if (size < 0 || !d->addressable())
        return false;
    if (isOpen())
        return d->impl.resize(size);
    const int fd = d->openFd(O_RDWR);
    if (fd < 0)
        return false;
    const bool ok = ::ftruncate(fd, size) == 0;
    ::close(fd);
    return ok;
}

qint64 File::readData(char *data, qint64 maxSize)
{
    const qint64 n = d->impl.read(data, maxSize);
    if (n < 0)
        setErrorString(d->impl.errorString());
    return n;
}

qint64 File::writeData(const char *data, qint64 maxSize)
{
    const qint64 n = d->impl.write(data, maxSize);
    if (n < 0)
        setErrorString(d->impl.errorString());
    return n;
}

bool File::seek(qint64 pos)
{
    return QIODevice::seek(pos) && d->impl.seek(pos);
}

} // namespace StorageKit
