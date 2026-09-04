// SPDX-License-Identifier: MIT
#include <StorageKit/FileSystem.h>

#include <QTemporaryDir>
#include <QTest>

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

using namespace StorageKit;

class tst_LocalBackend : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
        m_fs = std::make_unique<FileSystem>(QUrl::fromLocalFile(m_dir->path()));
        QVERIFY(m_fs->isValid());
    }

    void invalidRoot()
    {
        QVERIFY(!FileSystem(QUrl::fromLocalFile("/nonexistent-storagekit")).isValid());
        QVERIFY(!FileSystem(QUrl()).isValid());
        QVERIFY(!FileSystem().isValid());
    }

    void openWriteReadRoundtrip()
    {
        int fd = m_fs->open("a.txt", O_WRONLY | O_CREAT);
        QVERIFY(fd >= 0);
        QCOMPARE(::write(fd, "hello", 5), 5);
        QCOMPARE(::close(fd), 0);

        fd = m_fs->open("a.txt", O_RDONLY);
        QVERIFY(fd >= 0);
        char buf[16] = {};
        QCOMPARE(::read(fd, buf, sizeof(buf)), 5);
        QCOMPARE(QByteArray(buf), QByteArray("hello"));
        QCOMPARE(::close(fd), 0);
    }

    void openTruncAndAppend()
    {
        writeFile("a.txt", "0123456789");
        int fd = m_fs->open("a.txt", O_WRONLY | O_APPEND);
        QVERIFY(fd >= 0);
        QCOMPARE(::write(fd, "ab", 2), 2);
        ::close(fd);
        QCOMPARE(readFile("a.txt"), QByteArray("0123456789ab"));

        fd = m_fs->open("a.txt", O_WRONLY | O_TRUNC);
        QVERIFY(fd >= 0);
        ::close(fd);
        QCOMPARE(readFile("a.txt"), QByteArray());
    }

    void openErrors()
    {
        errno = 0;
        QCOMPARE(m_fs->open("missing.txt", O_RDONLY), -1);
        QCOMPARE(errno, ENOENT);

        writeFile("a.txt", "x");
        errno = 0;
        QCOMPARE(m_fs->open("a.txt", O_WRONLY | O_CREAT | O_EXCL), -1);
        QCOMPARE(errno, EEXIST);
    }

    void pathEscapeRejected()
    {
        errno = 0;
        QCOMPARE(m_fs->open("../evil.txt", O_WRONLY | O_CREAT), -1);
        QCOMPARE(errno, EACCES);
        errno = 0;
        QCOMPARE(m_fs->open("/etc/passwd", O_RDONLY), -1);
        QCOMPARE(errno, EACCES);
        errno = 0;
        QCOMPARE(m_fs->mkdir("a/../b"), -1);
        QCOMPARE(errno, EACCES);
        errno = 0;
        QCOMPARE(m_fs->stat("a//b", nullptr), -1);
        QCOMPARE(errno, EINVAL);
        QVERIFY(!m_fs->exists("../"));
    }

    void mkdirRmdir()
    {
        QCOMPARE(m_fs->mkdir("sub"), 0);
        QVERIFY(m_fs->exists("sub"));

        writeFile("sub/f.txt", "x");
        errno = 0;
        QCOMPARE(m_fs->rmdir("sub"), -1);
        QCOMPARE(errno, ENOTEMPTY);

        QCOMPARE(m_fs->unlink("sub/f.txt"), 0);
        QCOMPARE(m_fs->rmdir("sub"), 0);
        QVERIFY(!m_fs->exists("sub"));
    }

    void unlinkErrors()
    {
        QCOMPARE(m_fs->mkdir("sub"), 0);
        errno = 0;
        QVERIFY(m_fs->unlink("sub") == -1); // unlink on dir: EISDIR (Linux) / EPERM (POSIX/macOS)
        QVERIFY(errno == EISDIR || errno == EPERM);
        errno = 0;
        QCOMPARE(m_fs->unlink("missing"), -1);
        QCOMPARE(errno, ENOENT);
    }

    void renameAndTruncate()
    {
        writeFile("a.txt", "0123456789");
        QCOMPARE(m_fs->mkdir("sub"), 0);
        QCOMPARE(m_fs->rename("a.txt", "sub/b.txt"), 0);
        QVERIFY(!m_fs->exists("a.txt"));
        QCOMPARE(readFile("sub/b.txt"), QByteArray("0123456789"));

        QCOMPARE(m_fs->truncate("sub/b.txt", 4), 0);
        QCOMPARE(readFile("sub/b.txt"), QByteArray("0123"));
        errno = 0;
        QCOMPARE(m_fs->truncate("sub/b.txt", -1), -1);
        QCOMPARE(errno, EINVAL);
    }

    void statFile()
    {
        writeFile("a.txt", "hello");
        Stat st;
        QCOMPARE(m_fs->stat("a.txt", &st), 0);
        QCOMPARE(st.type, Stat::File);
        QCOMPARE(st.size, 5);
        QVERIFY(st.readable);
        QVERIFY(st.writable);
        QCOMPARE(st.mime, QStringLiteral("text/plain"));
        QVERIFY(st.mtime.isValid());

        QCOMPARE(m_fs->mkdir("sub"), 0);
        QCOMPARE(m_fs->stat("sub", &st), 0);
        QCOMPARE(st.type, Stat::Dir);

        QCOMPARE(m_fs->stat("", &st), 0); // root
        QCOMPARE(st.type, Stat::Dir);

        errno = 0;
        QCOMPARE(m_fs->stat("missing", &st), -1);
        QCOMPARE(errno, ENOENT);
    }

    void entryInfoListBasics()
    {
        writeFile("b.txt", "22");
        writeFile("a.md", "1");
        QCOMPARE(m_fs->mkdir("zdir"), 0);

        const FileInfoList all = m_fs->entryInfoList("", {}, QDir::NoFilter, QDir::Name);
        QCOMPARE(all.size(), 3);
        QCOMPARE(all[0].fileName(), QStringLiteral("a.md"));
        QCOMPARE(all[1].fileName(), QStringLiteral("b.txt"));
        QCOMPARE(all[2].fileName(), QStringLiteral("zdir"));
        QVERIFY(all[2].isDir());
        QCOMPARE(all[1].size(), 2);
        QCOMPARE(all[1].filePath(), QStringLiteral("b.txt"));
        QCOMPARE(all[1].suffix(), QStringLiteral("txt"));
        QCOMPARE(all[1].mimeType(), QStringLiteral("text/plain"));

        const FileInfoList filesOnly = m_fs->entryInfoList("", {}, QDir::Files, QDir::Name);
        QCOMPARE(filesOnly.size(), 2);

        const FileInfoList txt = m_fs->entryInfoList("", {QStringLiteral("*.txt")},
                                                     QDir::Files, QDir::Name);
        QCOMPARE(txt.size(), 1);
        QCOMPARE(txt[0].fileName(), QStringLiteral("b.txt"));

        const FileInfoList dirsFirst =
            m_fs->entryInfoList("", {}, QDir::NoFilter, QDir::Name | QDir::DirsFirst);
        QCOMPARE(dirsFirst[0].fileName(), QStringLiteral("zdir"));

        const FileInfoList reversed =
            m_fs->entryInfoList("", {}, QDir::NoFilter, QDir::Name | QDir::Reversed);
        QCOMPARE(reversed[0].fileName(), QStringLiteral("zdir"));

        QVERIFY(m_fs->entryInfoList("missing").isEmpty());
    }

    void entryInfoListHidden()
    {
        writeFile(".hidden", "x");
        writeFile("shown", "x");
        QCOMPARE(m_fs->entryInfoList("", {}, QDir::Files, QDir::Name).size(), 1);
        QCOMPARE(m_fs->entryInfoList("", {}, QDir::Files | QDir::Hidden, QDir::Name).size(), 2);
    }

    void urlInfo()
    {
        writeFile("a.txt", "hello");
        const FileInfo fi = FileSystem::urlInfo(QUrl::fromLocalFile(m_dir->path() + "/a.txt"));
        QVERIFY(fi.exists());
        QVERIFY(fi.isFile());
        QCOMPARE(fi.fileName(), QStringLiteral("a.txt"));
        QCOMPARE(fi.size(), 5);
        QCOMPARE(fi.mimeType(), QStringLiteral("text/plain"));

        const FileInfo dirInfo = FileSystem::urlInfo(QUrl::fromLocalFile(m_dir->path()));
        QVERIFY(dirInfo.exists());
        QVERIFY(dirInfo.isDir());

        QVERIFY(!FileSystem::urlInfo(QUrl::fromLocalFile("/nonexistent-storagekit")).exists());
        QVERIFY(!FileSystem::urlInfo(QUrl()).exists());
    }

    void openDeviceRoundtrip()
    {
        auto out = m_fs->openDevice("dev.txt", QIODevice::WriteOnly);
        QVERIFY(out);
        QCOMPARE(out->write("data"), 4);
        out.reset();

        auto in = m_fs->openDevice("dev.txt", QIODevice::ReadOnly);
        QVERIFY(in);
        QCOMPARE(in->readAll(), QByteArray("data"));
    }

private:
    void writeFile(const QString &path, const QByteArray &content)
    {
        const int fd = m_fs->open(path, O_WRONLY | O_CREAT | O_TRUNC);
        QVERIFY(fd >= 0);
        QCOMPARE(::write(fd, content.constData(), content.size()), qint64(content.size()));
        QCOMPARE(::close(fd), 0);
    }

    QByteArray readFile(const QString &path)
    {
        const int fd = m_fs->open(path, O_RDONLY);
        if (fd < 0)
            return {};
        QByteArray content;
        char buf[4096];
        ssize_t n;
        while ((n = ::read(fd, buf, sizeof(buf))) > 0)
            content.append(buf, n);
        ::close(fd);
        return content;
    }

    std::unique_ptr<QTemporaryDir> m_dir;
    std::unique_ptr<FileSystem> m_fs;
};

QTEST_GUILESS_MAIN(tst_LocalBackend)
#include "tst_localbackend.moc"
