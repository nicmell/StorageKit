// SPDX-License-Identifier: MIT
#pragma once

#include <StorageKit/Dir.h>
#include <StorageKit/FileInfo.h>

#include <QIODevice>
#include <QUrl>

namespace StorageKit {

/*!
 * A file in user-granted storage, mirroring QFile — and, like QFile, a real
 * QIODevice: open()/readAll()/write()/seek()/close() behave the same and a
 * File can be passed wherever a QIODevice& is expected.
 *
 * Two addressing modes:
 *  - File(dir, "notes/a.txt"): a file inside a granted Dir (may not exist
 *    yet; opening WriteOnly creates it, like QFile).
 *  - File(url): a standalone granted document, e.g. a FileDialog result.
 *
 * Errors are reported through errorString(). Blocking API — call from a
 * worker thread on Android (binder IPC).
 */
class File : public QIODevice
{
    Q_OBJECT

public:
    explicit File(const QUrl &url, QObject *parent = nullptr);
    File(const Dir &dir, const QString &fileName, QObject *parent = nullptr);
    ~File() override;

    bool open(QIODevice::OpenMode mode) override;
    void close() override;
    qint64 size() const override; // open: from the stream; closed: from fileInfo()
    bool isSequential() const override;

    bool exists() const;
    FileInfo fileInfo() const;
    QString fileName() const; // display name
    QUrl url() const;         // empty for files addressed through a Dir

    bool remove();
    bool rename(const QString &newName); // display name only, stays in place
    bool resize(qint64 size);

protected:
    qint64 readData(char *data, qint64 maxSize) override;
    qint64 writeData(const char *data, qint64 maxSize) override;
    bool seek(qint64 pos) override;

private:
    class Private;
    std::unique_ptr<Private> d;
};

} // namespace StorageKit
