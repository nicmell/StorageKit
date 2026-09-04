// SPDX-License-Identifier: MIT
// JNI plumbing over android.net.Uri, ContentResolver and DocumentsContract.
// Intent/DocumentsContract call patterns derived from KDAB's shared_storage
// wrapper (MIT, (C) 2022 Klarälvdalens Datakonsult AB, author BogDan Vatra).
#include "jniutils.h"

#include <QCoreApplication>
#include <QJniEnvironment>

namespace StorageKit {
namespace jni {

namespace {
constexpr int FLAG_GRANT_READ_URI_PERMISSION = 0x00000001;
constexpr int FLAG_GRANT_WRITE_URI_PERMISSION = 0x00000002;
} // namespace

QJniObject parseUri(const QString &uri)
{
    return QJniObject::callStaticObjectMethod("android/net/Uri", "parse",
                                              "(Ljava/lang/String;)Landroid/net/Uri;",
                                              QJniObject::fromString(uri).object());
}

QUrl toUrl(const QJniObject &uri)
{
    return uri.isValid() ? QUrl(uri.toString()) : QUrl{};
}

QJniObject context()
{
    return QNativeInterface::QAndroidApplication::context();
}

QJniObject contentResolver()
{
    return context().callObjectMethod("getContentResolver", "()Landroid/content/ContentResolver;");
}

QJniObject stringArray(const QStringList &list)
{
    QJniEnvironment env;
    jobjectArray array = env->NewObjectArray(list.size(), env->FindClass("java/lang/String"), nullptr);
    for (int i = 0; i < list.size(); ++i)
        env->SetObjectArrayElement(array, i, QJniObject::fromString(list[i]).object());
    return QJniObject::fromLocalRef(array);
}

QString treeDocumentId(const QJniObject &treeUri)
{
    const auto id = QJniObject::callStaticObjectMethod(
        "android/provider/DocumentsContract", "getTreeDocumentId",
        "(Landroid/net/Uri;)Ljava/lang/String;", treeUri.object());
    return id.isValid() ? id.toString() : QString{};
}

QJniObject buildChildDocumentsUriUsingTree(const QJniObject &treeUri, const QString &parentDocId)
{
    return QJniObject::callStaticObjectMethod(
        "android/provider/DocumentsContract", "buildChildDocumentsUriUsingTree",
        "(Landroid/net/Uri;Ljava/lang/String;)Landroid/net/Uri;", treeUri.object(),
        QJniObject::fromString(parentDocId).object());
}

QJniObject buildDocumentUriUsingTree(const QJniObject &treeUri, const QString &docId)
{
    return QJniObject::callStaticObjectMethod(
        "android/provider/DocumentsContract", "buildDocumentUriUsingTree",
        "(Landroid/net/Uri;Ljava/lang/String;)Landroid/net/Uri;", treeUri.object(),
        QJniObject::fromString(docId).object());
}

QJniObject createDocument(const QJniObject &parentDocUri, const QString &mimeType, const QString &displayName)
{
    return QJniObject::callStaticObjectMethod(
        "android/provider/DocumentsContract", "createDocument",
        "(Landroid/content/ContentResolver;Landroid/net/Uri;Ljava/lang/String;Ljava/lang/String;)Landroid/net/Uri;",
        contentResolver().object(), parentDocUri.object(),
        QJniObject::fromString(mimeType).object(), QJniObject::fromString(displayName).object());
}

bool deleteDocument(const QJniObject &docUri)
{
    return QJniObject::callStaticMethod<jboolean>(
        "android/provider/DocumentsContract", "deleteDocument",
        "(Landroid/content/ContentResolver;Landroid/net/Uri;)Z", contentResolver().object(),
        docUri.object());
}

QJniObject renameDocument(const QJniObject &docUri, const QString &displayName)
{
    return QJniObject::callStaticObjectMethod(
        "android/provider/DocumentsContract", "renameDocument",
        "(Landroid/content/ContentResolver;Landroid/net/Uri;Ljava/lang/String;)Landroid/net/Uri;",
        contentResolver().object(), docUri.object(), QJniObject::fromString(displayName).object());
}

QJniObject moveDocument(const QJniObject &srcDocUri, const QJniObject &srcParentDocUri, const QJniObject &dstParentDocUri)
{
    return QJniObject::callStaticObjectMethod(
        "android/provider/DocumentsContract", "moveDocument",
        "(Landroid/content/ContentResolver;Landroid/net/Uri;Landroid/net/Uri;Landroid/net/Uri;)Landroid/net/Uri;",
        contentResolver().object(), srcDocUri.object(), srcParentDocUri.object(),
        dstParentDocUri.object());
}

int openDetachedFd(const QJniObject &uri, const QString &mode)
{
    const auto pfd = contentResolver().callObjectMethod(
        "openFileDescriptor",
        "(Landroid/net/Uri;Ljava/lang/String;)Landroid/os/ParcelFileDescriptor;", uri.object(),
        QJniObject::fromString(mode).object());
    if (!pfd.isValid())
        return -1;
    return pfd.callMethod<jint>("detachFd", "()I");
}

void takePersistableUriPermission(const QJniObject &uri)
{
    contentResolver().callMethod<void>("takePersistableUriPermission", "(Landroid/net/Uri;I)V",
                                       uri.object(),
                                       FLAG_GRANT_READ_URI_PERMISSION | FLAG_GRANT_WRITE_URI_PERMISSION);
}

void releasePersistableUriPermission(const QJniObject &uri)
{
    contentResolver().callMethod<void>("releasePersistableUriPermission", "(Landroid/net/Uri;I)V",
                                       uri.object(),
                                       FLAG_GRANT_READ_URI_PERMISSION | FLAG_GRANT_WRITE_URI_PERMISSION);
}

bool hasPersistedUriPermission(const QUrl &uri)
{
    const QString wanted = uri.toString(QUrl::FullyEncoded);
    const auto perms = contentResolver().callObjectMethod("getPersistedUriPermissions",
                                                          "()Ljava/util/List;");
    if (!perms.isValid())
        return false;
    const int count = perms.callMethod<jint>("size", "()I");
    for (int i = 0; i < count; ++i) {
        const auto perm = perms.callObjectMethod("get", "(I)Ljava/lang/Object;", i);
        const auto permUri = perm.callObjectMethod("getUri", "()Landroid/net/Uri;");
        if (permUri.isValid() && permUri.toString() == wanted)
            return true;
    }
    return false;
}

} // namespace jni
} // namespace StorageKit
