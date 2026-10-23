// SPDX-License-Identifier: MIT
#include <StorageKit/FileDialog.h>

#include "backend_p.h"

#include <QCoreApplication>
#include <QFile>
#include <QFileDialog>
#include <QPromise>

#ifdef Q_OS_ANDROID
#include "android/jniutils.h"
#endif

namespace StorageKit {

namespace {

void persistGrant(const QUrl &url)
{
#ifdef Q_OS_ANDROID
    if (url.scheme() == QLatin1String("content"))
        jni::takePersistableUriPermission(jni::parseUri(url));
#else
    Q_UNUSED(url)
#endif
}

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

QList<QUrl> pickExisting(const QStringList &mimeTypes, QFileDialog::FileMode fileMode)
{
    QFileDialog dialog;
    dialog.setFileMode(fileMode);
    if (!mimeTypes.isEmpty())
        dialog.setMimeTypeFilters(mimeTypes);
    if (dialog.exec() != QDialog::Accepted)
        return {};
    const QList<QUrl> urls = dialog.selectedUrls();
    for (const QUrl &url : urls)
        persistGrant(url);
    return urls;
}

} // namespace

QFuture<QUrl> FileDialog::getExistingDirectoryUrl()
{
    return runOnGuiThread<QUrl>([] {
        const QUrl url = QFileDialog::getExistingDirectoryUrl();
        persistGrant(url);
        return url;
    });
}

QFuture<QUrl> FileDialog::getOpenFileUrl(const QStringList &mimeTypes)
{
    return runOnGuiThread<QUrl>([mimeTypes] {
        const QList<QUrl> urls = pickExisting(mimeTypes, QFileDialog::ExistingFile);
        return urls.isEmpty() ? QUrl{} : urls.first();
    });
}

QFuture<QList<QUrl>> FileDialog::getOpenFileUrls(const QStringList &mimeTypes)
{
    return runOnGuiThread<QList<QUrl>>([mimeTypes] {
        return pickExisting(mimeTypes, QFileDialog::ExistingFiles);
    });
}

QFuture<QUrl> FileDialog::getSaveFileUrl(const QString &suggestedName, const QString &mimeType)
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
        const QUrl url = dialog.selectedUrls().first();
#ifndef Q_OS_ANDROID
        QFile file(url.toLocalFile());
        if (!file.exists() && !file.open(QIODevice::WriteOnly))
            return QUrl{};
#endif
        persistGrant(url);
        return url;
    });
}

void FileDialog::releaseGrant(const QUrl &url)
{
    StorageKit::releaseGrant(url);
}


} // namespace StorageKit
