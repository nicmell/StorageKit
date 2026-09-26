// SPDX-License-Identifier: MIT
#include <StorageKit/Dir.h>
#include <StorageKit/File.h>

#include <QTemporaryDir>
#include <QTest>

using namespace StorageKit;

class tst_LocalBackend : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        m_tmp = std::make_unique<QTemporaryDir>();
        QVERIFY(m_tmp->isValid());
        m_dir = std::make_unique<Dir>(QUrl::fromLocalFile(m_tmp->path()));
        QVERIFY(m_dir->exists());
    }

    void invalidRoot()
    {
        QVERIFY(!Dir(QUrl::fromLocalFile("/nonexistent-storagekit")).exists());
        QVERIFY(!Dir(QUrl()).exists());
    }

    void fileWriteReadRoundtrip()
    {
        File out(*m_dir, "a.txt");
        QVERIFY(out.open(QIODevice::WriteOnly)); // creates, like QFile
        QCOMPARE(out.write("hello"), 5);
        out.close();

        File in(*m_dir, "a.txt");
        QVERIFY(in.open(QIODevice::ReadOnly));
        QCOMPARE(in.readAll(), QByteArray("hello"));
        QCOMPARE(in.size(), 5);
        in.close();
        QCOMPARE(in.size(), 5); // closed: from fileInfo()
    }

    void fileAppendTruncateSeek()
    {
        writeFile("a.txt", "0123456789");

        File file(*m_dir, "a.txt");
        QVERIFY(file.open(QIODevice::Append));
        QCOMPARE(file.write("ab"), 2);
        file.close();
        QCOMPARE(readFile("a.txt"), QByteArray("0123456789ab"));

        QVERIFY(file.open(QIODevice::ReadOnly));
        QVERIFY(file.seek(10));
        QCOMPARE(file.readAll(), QByteArray("ab"));
        file.close();

        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        file.close();
        QCOMPARE(readFile("a.txt"), QByteArray());
    }

    void fileErrors()
    {
        File missing(*m_dir, "missing.txt");
        QVERIFY(!missing.open(QIODevice::ReadOnly)); // read does not create
        QVERIFY(!missing.errorString().isEmpty());
        QVERIFY(!missing.exists());

        File escape(*m_dir, "../evil.txt");
        QVERIFY(!escape.open(QIODevice::WriteOnly));
        QVERIFY(!escape.exists());

        File absolute(*m_dir, "/etc/passwd");
        QVERIFY(!absolute.open(QIODevice::ReadOnly));
    }

    void fileRemoveRenameResize()
    {
        writeFile("a.txt", "0123456789");

        File file(*m_dir, "a.txt");
        QVERIFY(file.resize(4));
        QCOMPARE(readFile("a.txt"), QByteArray("0123"));
        QVERIFY(!file.resize(-1));

        QVERIFY(file.rename("b.txt"));
        QCOMPARE(file.fileName(), QStringLiteral("b.txt"));
        QVERIFY(!m_dir->exists("a.txt"));
        QVERIFY(m_dir->exists("b.txt"));
        QVERIFY(!file.rename("into/sub.txt")); // display name only

        QVERIFY(file.remove());
        QVERIFY(!m_dir->exists("b.txt"));
        QVERIFY(!file.remove()); // already gone
    }

    void fileInfo()
    {
        writeFile("a.txt", "hello");
        const FileInfo fi = File(*m_dir, "a.txt").fileInfo();
        QVERIFY(fi.exists());
        QVERIFY(fi.isFile());
        QCOMPARE(fi.fileName(), QStringLiteral("a.txt"));
        QCOMPARE(fi.size(), 5);
        QCOMPARE(fi.mimeType(), QStringLiteral("text/plain"));
        QVERIFY(fi.lastModified().isValid());

        QVERIFY(!File(*m_dir, "missing").fileInfo().exists());
    }

    void standaloneUrlFile()
    {
        writeFile("a.txt", "hello");
        const QUrl url = QUrl::fromLocalFile(m_tmp->path() + "/a.txt");

        File file(url);
        QVERIFY(file.exists());
        QCOMPARE(file.fileName(), QStringLiteral("a.txt"));
        QCOMPARE(file.url(), url);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("hello"));
        file.close();

        QVERIFY(file.rename("c.txt"));
        QVERIFY(file.url().toLocalFile().endsWith("/c.txt"));
        QVERIFY(file.exists());
        QVERIFY(file.remove());
        QVERIFY(!file.exists());
    }

    void dirMkdirRmdir()
    {
        QVERIFY(m_dir->mkdir("sub"));
        QVERIFY(m_dir->exists("sub"));
        QVERIFY(!m_dir->mkdir("sub")); // already there

        writeFile("sub/f.txt", "x");
        QVERIFY(!m_dir->rmdir("sub")); // not empty
        QVERIFY(m_dir->remove("sub/f.txt"));
        QVERIFY(m_dir->rmdir("sub"));
        QVERIFY(!m_dir->exists("sub"));
    }

    void dirCd()
    {
        QVERIFY(m_dir->mkdir("a"));
        QVERIFY(m_dir->mkdir("a/b"));
        writeFile("a/b/f.txt", "x");

        Dir dir = *m_dir;
        QCOMPARE(dir.path(), QString());
        QVERIFY(!dir.cdUp()); // root is the ceiling
        QVERIFY(!dir.cd(".."));

        QVERIFY(dir.cd("a"));
        QCOMPARE(dir.path(), QStringLiteral("a"));
        QCOMPARE(dir.dirName(), QStringLiteral("a"));
        QVERIFY(dir.cd("b"));
        QCOMPARE(dir.path(), QStringLiteral("a/b"));
        QVERIFY(dir.exists("f.txt"));
        QCOMPARE(dir.entryInfoList().size(), 1);

        QVERIFY(dir.cdUp());
        QCOMPARE(dir.path(), QStringLiteral("a"));
        QVERIFY(dir.cd(".."));   // == cdUp inside the grant
        QCOMPARE(dir.path(), QString());

        QVERIFY(!dir.cd("missing"));
        QVERIFY(dir.cd("a/b")); // multi-segment, like QDir
        QCOMPARE(dir.path(), QStringLiteral("a/b"));
    }

    void dirRename()
    {
        writeFile("a.txt", "x");
        QVERIFY(m_dir->mkdir("sub"));
        QVERIFY(m_dir->rename("a.txt", "sub/b.txt"));
        QVERIFY(!m_dir->exists("a.txt"));
        QCOMPARE(readFile("sub/b.txt"), QByteArray("x"));
    }

    void entryInfoListBasics()
    {
        writeFile("b.txt", "22");
        writeFile("a.md", "1");
        QVERIFY(m_dir->mkdir("zdir"));

        const FileInfoList all = m_dir->entryInfoList({}, QDir::NoFilter, QDir::Name);
        QCOMPARE(all.size(), 3);
        QCOMPARE(all[0].fileName(), QStringLiteral("a.md"));
        QCOMPARE(all[1].fileName(), QStringLiteral("b.txt"));
        QCOMPARE(all[2].fileName(), QStringLiteral("zdir"));
        QVERIFY(all[2].isDir());
        QCOMPARE(all[1].size(), 2);
        QCOMPARE(all[1].suffix(), QStringLiteral("txt"));

        QCOMPARE(m_dir->entryInfoList({}, QDir::Files, QDir::Name).size(), 2);

        const FileInfoList txt = m_dir->entryInfoList({QStringLiteral("*.txt")},
                                                      QDir::Files, QDir::Name);
        QCOMPARE(txt.size(), 1);
        QCOMPARE(txt[0].fileName(), QStringLiteral("b.txt"));

        QCOMPARE(m_dir->entryInfoList({}, QDir::NoFilter, QDir::Name | QDir::DirsFirst)[0]
                     .fileName(),
                 QStringLiteral("zdir"));
        QCOMPARE(m_dir->entryInfoList({}, QDir::NoFilter, QDir::Name | QDir::Reversed)[0]
                     .fileName(),
                 QStringLiteral("zdir"));
    }

    void entryInfoListHidden()
    {
        writeFile(".hidden", "x");
        writeFile("shown", "x");
        QCOMPARE(m_dir->entryInfoList({}, QDir::Files, QDir::Name).size(), 1);
        QCOMPARE(m_dir->entryInfoList({}, QDir::Files | QDir::Hidden, QDir::Name).size(), 2);
    }

private:
    void writeFile(const QString &path, const QByteArray &content)
    {
        File file(*m_dir, path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Truncate));
        QCOMPARE(file.write(content), qint64(content.size()));
    }

    QByteArray readFile(const QString &path)
    {
        File file(*m_dir, path);
        if (!file.open(QIODevice::ReadOnly))
            return {};
        return file.readAll();
    }

    std::unique_ptr<QTemporaryDir> m_tmp;
    std::unique_ptr<Dir> m_dir;
};

QTEST_GUILESS_MAIN(tst_LocalBackend)
#include "tst_localbackend.moc"
