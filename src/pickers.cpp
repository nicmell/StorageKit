// SPDX-License-Identifier: MIT
// One QFileDialog-based picker implementation for every platform: Qt's QPA
// helper shows the native dialog (the SAF pickers on Android). On Android a
// persistable permission is additionally taken on each resulting content URI
// ourselves, so granted locations survive app restarts by library contract,
// independent of what the platform helper does internally.
#include "filesystem_p.h"

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

// Dialogs must run on the GUI thread; hop there and settle the promise from
// the dialog result.
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

QFuture<QUrl> FileSystem::pickFolder()
{
    return runOnGuiThread<QUrl>([] {
        const QUrl url = QFileDialog::getExistingDirectoryUrl();
        persistGrant(url);
        return url;
    });
}

QFuture<QUrl> FileSystem::pickFile(const QStringList &mimeTypes)
{
    return runOnGuiThread<QUrl>([mimeTypes] {
        const QList<QUrl> urls = pickExisting(mimeTypes, QFileDialog::ExistingFile);
        return urls.isEmpty() ? QUrl{} : urls.first();
    });
}

QFuture<QList<QUrl>> FileSystem::pickFiles(const QStringList &mimeTypes)
{
    return runOnGuiThread<QList<QUrl>>([mimeTypes] {
        return pickExisting(mimeTypes, QFileDialog::ExistingFiles);
    });
}

QFuture<QUrl> FileSystem::pickSaveFile(const QString &suggestedName, const QString &mimeType)
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
        // ACTION_CREATE_DOCUMENT creates the document; match that on desktop
        // so the returned URL always points at an existing file.
        QFile file(url.toLocalFile());
        if (!file.exists())
            file.open(QIODevice::WriteOnly);
#endif
        persistGrant(url);
        return url;
    });
}

} // namespace StorageKit
