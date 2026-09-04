// SPDX-License-Identifier: MIT
// Storage Access Framework backend. Documents are addressed by opaque
// document ids; relative paths are resolved by walking child listings from
// the granted tree root, with a docId cache invalidated on every mutation.
#include "filesystem_p.h"

#include "fileinfo_p.h"
#include "jniutils.h"

#include <QFileInfo>
#include <QJniEnvironment>
#include <QMimeDatabase>
#include <QMutex>

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace StorageKit {
    namespace {
        const QLatin1String MIME_DIR("vnd.android.document/directory");
        constexpr int FLAG_SUPPORTS_WRITE = 0x00000002;

        struct DocEntry {
            QString docId;
            QString name;
            QString mime;
            qint64 size = 0;
            qint64 mtime = 0;
            int flags = 0;

            bool isDir() const { return mime == MIME_DIR; }
        };

        // One cursor query over a document/children uri; empty projection-driven rows.
        QList<DocEntry> queryUri(const QJniObject &uri) {
            QList<DocEntry> result;
            const auto projection = jni::stringArray({
                QStringLiteral("document_id"),
                QStringLiteral("_display_name"),
                QStringLiteral("mime_type"),
                QStringLiteral("_size"),
                QStringLiteral("last_modified"),
                QStringLiteral("flags")
            });
            const auto cursor = jni::contentResolver().callObjectMethod(
                "query",
                "(Landroid/net/Uri;[Ljava/lang/String;Ljava/lang/String;[Ljava/lang/String;Ljava/lang/String;)Landroid/database/Cursor;",
                uri.object(), projection.object(), nullptr, nullptr, nullptr);
            if (!cursor.isValid())
                return result;
            while (cursor.callMethod<jboolean>("moveToNext", "()Z")) {
                DocEntry e;
                auto getString = [&](int col) {
                    const auto s = cursor.callObjectMethod("getString", "(I)Ljava/lang/String;", col);
                    return s.isValid() ? s.toString() : QString{};
                };
                e.docId = getString(0);
                e.name = getString(1);
                e.mime = getString(2);
                e.size = cursor.callMethod<jlong>("getLong", "(I)J", 3);
                e.mtime = cursor.callMethod<jlong>("getLong", "(I)J", 4);
                e.flags = cursor.callMethod<jint>("getInt", "(I)I", 5);
                result.append(e);
            }
            cursor.callMethod<void>("close", "()V");
            return result;
        }

        FileInfo makeInfo(const DocEntry &e, const QString &relPath) {
            auto *data = new FileInfoData;
            data->name = e.name;
            data->filePath = relPath;
            data->size = e.isDir() ? 0 : e.size;
            data->dir = e.isDir();
            data->present = true;
            data->mtime = QDateTime::fromMSecsSinceEpoch(e.mtime);
            data->readable = true; // listing it implies read access under the grant
            data->writable = e.flags & FLAG_SUPPORTS_WRITE;
            data->mime = e.mime;
            return FileInfo(data);
        }

        QString guessMime(const QString &name) {
            static QMimeDatabase db;
            const auto type = db.mimeTypeForFile(name, QMimeDatabase::MatchExtension);
            return type.isDefault() ? QStringLiteral("application/octet-stream") : type.name();
        }

        QString parentOf(const QString &path) {
            const int slash = path.lastIndexOf(u'/');
            return slash < 0 ? QString{} : path.left(slash);
        }

        QString nameOf(const QString &path) {
            const int slash = path.lastIndexOf(u'/');
            return slash < 0 ? path : path.mid(slash + 1);
        }

        class SafBackend : public Backend {
        public:
            explicit SafBackend(const QUrl &root)
                : m_root(root)
                  , m_rootUri(jni::parseUri(root)) {
            }

            bool isValid() const override {
                if (!m_root.isValid() || m_root.scheme() != QLatin1String("content"))
                    return false;
                if (!jni::hasPersistedUriPermission(m_root))
                    return false;
                Stat st;
                return const_cast<SafBackend *>(this)->statOne({}, &st) == 0;
            }

            QUrl root() const override { return m_root; }

            int openFd(const QString &path, int flags) override {
                QString docId = resolve(path);
                if (docId.isEmpty()) {
                    if (!(flags & O_CREAT))
                        return fail(ENOENT);
                    const QString parentId = resolve(parentOf(path));
                    if (parentId.isEmpty())
                        return fail(ENOENT);
                    const auto uri = jni::createDocument(docUri(parentId), guessMime(nameOf(path)),
                                                         nameOf(path));
                    if (!uri.isValid())
                        return fail(EACCES);
                    invalidate();
                    docId = resolve(path);
                    if (docId.isEmpty())
                        return fail(EIO);
                } else if ((flags & O_CREAT) && (flags & O_EXCL)) {
                    return fail(EEXIST);
                }

                QString mode;
                const bool append = flags & O_APPEND;
                bool emulateAppend = false;
                switch (flags & O_ACCMODE) {
                    case O_RDONLY:
                        mode = QStringLiteral("r");
                        break;
                    case O_WRONLY:
                        if (flags & O_TRUNC)
                            mode = QStringLiteral("wt");
                        else if (append)
                            mode = QStringLiteral("wa");
                        else
                            mode = QStringLiteral("w");
                        break;
                    case O_RDWR:
                        mode = (flags & O_TRUNC) ? QStringLiteral("rwt") : QStringLiteral("rw");
                        emulateAppend = append;
                        break;
                    default:
                        return fail(EINVAL);
                }

                const int fd = jni::openDetachedFd(docUri(docId), mode);
                if (fd < 0)
                    return fail(EACCES);
                if (emulateAppend)
                    ::lseek(fd, 0, SEEK_END);
                return fd;
            }

            int statOne(const QString &path, Stat *out) override {
                const QString docId = resolve(path);
                if (docId.isEmpty())
                    return fail(ENOENT);
                const auto rows = queryUri(docUri(docId));
                if (rows.isEmpty())
                    return fail(ENOENT);
                if (out) {
                    const DocEntry &e = rows.first();
                    out->type = e.isDir() ? Stat::Dir : Stat::File;
                    out->size = e.isDir() ? 0 : e.size;
                    out->mtime = QDateTime::fromMSecsSinceEpoch(e.mtime);
                    out->mime = e.mime;
                    out->readable = true;
                    out->writable = e.flags & FLAG_SUPPORTS_WRITE;
                }
                return 0;
            }

            int mkdir(const QString &path) override {
                if (!resolve(path).isEmpty())
                    return fail(EEXIST);
                const QString parentId = resolve(parentOf(path));
                if (parentId.isEmpty())
                    return fail(ENOENT);
                const auto uri = jni::createDocument(docUri(parentId), MIME_DIR, nameOf(path));
                if (!uri.isValid())
                    return fail(EACCES);
                invalidate();
                return 0;
            }

            int rmdir(const QString &path) override {
                return removeDocument(path, /*expectDir=*/true);
            }

            int unlink(const QString &path) override {
                return removeDocument(path, /*expectDir=*/false);
            }

            int rename(const QString &from, const QString &to) override {
                const QString fromId = resolve(from);
                if (fromId.isEmpty())
                    return fail(ENOENT);
                if (!resolve(to).isEmpty())
                    return fail(EEXIST);

                const QString fromParent = parentOf(from);
                const QString toParent = parentOf(to);
                QString currentId = fromId;

                if (fromParent != toParent) {
                    const QString srcParentId = resolve(fromParent);
                    const QString dstParentId = resolve(toParent);
                    if (dstParentId.isEmpty())
                        return fail(ENOENT);
                    const auto moved = jni::moveDocument(docUri(currentId), docUri(srcParentId),
                                                         docUri(dstParentId));
                    if (!moved.isValid())
                        return fail(EACCES);
                    invalidate();
                    currentId = resolve(toParent.isEmpty()
                                            ? nameOf(from)
                                            : toParent + u'/' + nameOf(from));
                    if (currentId.isEmpty())
                        return fail(EIO);
                }
                if (nameOf(from) != nameOf(to)) {
                    const auto renamed = jni::renameDocument(docUri(currentId), nameOf(to));
                    if (!renamed.isValid())
                        return fail(EACCES);
                }
                invalidate();
                return 0;
            }

            int truncate(const QString &path, qint64 length) override {
                const int fd = openFd(path, O_RDWR);
                if (fd < 0)
                    return -1;
                const int res = ::ftruncate(fd, length);
                ::close(fd);
                return res;
            }

            bool exists(const QString &path) override {
                return !resolve(path).isEmpty();
            }

            int rawList(const QString &path, FileInfoList *out) override {
                Stat st;
                if (statOne(path, &st) != 0)
                    return -1;
                if (st.type != Stat::Dir)
                    return fail(ENOTDIR);
                const QString dirId = resolve(path);
                const auto rows = queryUri(jni::buildChildDocumentsUriUsingTree(m_rootUri, dirId));
                QMutexLocker locker(&m_mutex);
                for (const DocEntry &e: rows) {
                    const QString rel = path.isEmpty() ? e.name : path + u'/' + e.name;
                    m_docIds.insert(rel, e.docId);
                    out->append(makeInfo(e, rel));
                }
                return 0;
            }

        private:
            QJniObject docUri(const QString &docId) const {
                return jni::buildDocumentUriUsingTree(m_rootUri, docId);
            }

            static int fail(int err) {
                errno = err;
                return -1;
            }

            // relPath -> docId, walking cached segments; "" resolves to the tree root.
            QString resolve(const QString &path) {
                if (path.isEmpty())
                    return jni::treeDocumentId(m_rootUri);
                {
                    QMutexLocker locker(&m_mutex);
                    if (const auto it = m_docIds.constFind(path); it != m_docIds.constEnd())
                        return *it;
                }
                const QString parentId = resolve(parentOf(path));
                if (parentId.isEmpty())
                    return {};
                const QString wanted = nameOf(path);
                const auto rows = queryUri(jni::buildChildDocumentsUriUsingTree(m_rootUri, parentId));
                QMutexLocker locker(&m_mutex);
                QString found;
                for (const DocEntry &e: rows) {
                    const QString rel = parentOf(path).isEmpty() ? e.name : parentOf(path) + u'/' + e.name;
                    m_docIds.insert(rel, e.docId);
                    if (e.name == wanted && found.isEmpty())
                        found = e.docId; // first match wins on duplicate display names
                }
                return found;
            }

            int removeDocument(const QString &path, bool expectDir) {
                const QString docId = resolve(path);
                if (docId.isEmpty())
                    return fail(ENOENT);
                Stat st;
                if (statOne(path, &st) != 0)
                    return -1;
                if (expectDir && st.type != Stat::Dir)
                    return fail(ENOTDIR);
                if (!expectDir && st.type == Stat::Dir)
                    return fail(EISDIR);
                if (expectDir) {
                    const auto children = queryUri(jni::buildChildDocumentsUriUsingTree(m_rootUri, docId));
                    if (!children.isEmpty())
                        return fail(ENOTEMPTY);
                }
                if (!jni::deleteDocument(docUri(docId)))
                    return fail(EACCES);
                invalidate();
                return 0;
            }

            void invalidate() {
                QMutexLocker locker(&m_mutex);
                m_docIds.clear();
            }

            QUrl m_root;
            QJniObject m_rootUri;
            mutable QMutex m_mutex;
            QHash<QString, QString> m_docIds;
        };

    } // namespace

    std::shared_ptr<Backend> createBackend(const QUrl &root) {
        return std::make_shared<SafBackend>(root);
    }

    int platformOpenUrl(const QUrl &url, int flags) {
        QString mode;
        switch (flags & O_ACCMODE) {
            case O_RDONLY:
                mode = QStringLiteral("r");
                break;
            case O_WRONLY:
                mode = (flags & O_TRUNC) ? QStringLiteral("wt") : ((flags & O_APPEND) ? QStringLiteral("wa") : QStringLiteral("w"));
                break;
            case O_RDWR:
                mode = (flags & O_TRUNC) ? QStringLiteral("rwt") : QStringLiteral("rw");
                break;
            default:
                errno = EINVAL;
                return -1;
        }
        const int fd = jni::openDetachedFd(jni::parseUri(url), mode);
        if (fd < 0)
            errno = EACCES;
        if (fd >= 0 && (flags & O_APPEND) && (flags & O_ACCMODE) == O_RDWR)
            ::lseek(fd, 0, SEEK_END);
        return fd;
    }

    FileInfo platformUrlInfo(const QUrl &url) {
        if (url.scheme() != QLatin1String("content")) {
            // file:// URLs (app-private storage) still resolve locally.
            const QString path = url.isLocalFile() ? url.toLocalFile() : QString{};
            if (path.isEmpty() || !QFileInfo::exists(path))
                return {};
            const QFileInfo qfi(path);
            auto *data = new FileInfoData;
            data->name = qfi.fileName();
            data->filePath = qfi.absoluteFilePath();
            data->size = qfi.isDir() ? 0 : qfi.size();
            data->dir = qfi.isDir();
            data->present = true;
            data->mtime = qfi.lastModified();
            data->readable = qfi.isReadable();
            data->writable = qfi.isWritable();
            return FileInfo(data);
        }

        QJniObject uri = jni::parseUri(url);
        if (!jni::isDocumentUri(uri)) {
            // Tree URL from the folder picker: address its root document.
            const QString docId = jni::treeDocumentId(uri);
            if (docId.isEmpty())
                return {};
            uri = jni::buildDocumentUriUsingTree(uri, docId);
        }
        const auto rows = queryUri(uri);
        if (rows.isEmpty())
            return {};
        return makeInfo(rows.first(), url.toString());
    }

    void platformReleaseGrant(const QUrl &url) {
        jni::releasePersistableUriPermission(jni::parseUri(url));
    }
} // namespace StorageKit
