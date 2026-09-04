// SPDX-License-Identifier: MIT
#pragma once

#include <QJniObject>
#include <QStringList>
#include <QUrl>

namespace StorageKit {
namespace jni {

// android.net.Uri wrappers
QJniObject parseUri(const QString &uri);
inline QJniObject parseUri(const QUrl &url) { return parseUri(url.toString(QUrl::FullyEncoded)); }
QUrl toUrl(const QJniObject &uri);

QJniObject context();
QJniObject contentResolver();
QJniObject stringArray(const QStringList &list);

// DocumentsContract statics
bool isDocumentUri(const QJniObject &uri);
QString treeDocumentId(const QJniObject &treeUri);
QJniObject buildChildDocumentsUriUsingTree(const QJniObject &treeUri, const QString &parentDocId);
QJniObject buildDocumentUriUsingTree(const QJniObject &treeUri, const QString &docId);
QJniObject createDocument(const QJniObject &parentDocUri, const QString &mimeType, const QString &displayName);
bool deleteDocument(const QJniObject &docUri);
QJniObject renameDocument(const QJniObject &docUri, const QString &displayName);
QJniObject moveDocument(const QJniObject &srcDocUri, const QJniObject &srcParentDocUri, const QJniObject &dstParentDocUri);

// ContentResolver.openFileDescriptor(uri, mode) -> detached native fd, or -1.
int openDetachedFd(const QJniObject &uri, const QString &mode);

// Persistable grants
void takePersistableUriPermission(const QJniObject &uri);
void releasePersistableUriPermission(const QJniObject &uri);
bool hasPersistedUriPermission(const QUrl &uri);

} // namespace jni
} // namespace StorageKit
