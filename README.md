# StorageKit

A small Qt 6 library giving desktop and Android QML applications uniform access
to user-granted storage, through classes shaped like Qt's own file APIs —
if you know `QDir`, `QFile` and `QFileDialog`, you already know StorageKit.

On Android, apps cannot browse the filesystem: access goes through the
[Storage Access Framework](https://developer.android.com/guide/topics/providers/document-provider)
(SAF) — the user picks a folder or file in a system dialog and the app receives
an opaque `content://` URI valid only under that grant. Qt 6 covers parts of
this (native picker, `content://` reads through `QFile`), but leaves gaps: no
persistable permissions, no create/rename/move/delete inside a picked tree, no
batched metadata. StorageKit fills those gaps and gives desktop the same API on
top of plain directories. It is a modernized take on
[KDAB's shared storage wrapper](https://www.kdab.com/android-shared-storage-qt-wrapper/)
(MIT, © 2022 Klarälvdalens Datakonsult AB), whose DocumentsContract JNI
patterns it reuses.

## Qt parity

| Qt | StorageKit | Deviations |
|---|---|---|
| `QDir` | `Dir` | built from a granted `QUrl`; cannot leave the granted root |
| `QFile` | `File` | built from a `QUrl` or `(Dir, name)`; it *is* a `QIODevice` |
| `QFileInfo` | `FileInfo` | subset, plus `mimeType()` |
| `QFileDialog` statics | `FileDialog` statics | async (`QFuture`); MIME filters instead of name filters |

```cpp
#include <StorageKit/Dir.h>
#include <StorageKit/File.h>
#include <StorageKit/FileDialog.h>

using namespace StorageKit;

// First run: the user grants a folder (SAF tree picker on Android).
FileDialog::getExistingDirectoryUrl().then([](QUrl root) {
    if (root.isEmpty())
        return; // canceled
    settings.setValue("root", root); // persist the grant

    Dir dir(root);
    dir.mkdir("notes");

    File file(dir, "notes/todo.txt");
    file.open(QIODevice::WriteOnly);  // creates, exactly like QFile
    file.write("hello\n");
    file.close();

    for (const FileInfo &fi : dir.entryInfoList({"*.txt"}, QDir::Files, QDir::Name))
        qDebug() << fi.fileName() << fi.size() << fi.mimeType();
});

// Every later launch: silent, no UI.
Dir dir(settings.value("root").toUrl());
if (!dir.exists())
    ; // grant revoked -> pick again
```

`File` is a real `QIODevice`: pass it to anything expecting a `QIODevice&`,
use `readAll()/write()/seek()/atEnd()` as with `QFile`. A `File` can also wrap
a standalone granted document — the result of `getOpenFileUrl()` or
`getSaveFileUrl()` — via `File(url)`, with `exists()/fileInfo()/remove()/
rename()/resize()` working without opening.

## The capability model

One `Dir` = one granted root: a directory (`file://`) on desktop, a SAF tree
URI (`content://`) on Android. Navigation stays inside it — `cd("..")` at the
root fails, absolute paths are rejected. Persist `rootUrl()` and rebuild with
the constructor next launch: the pickers take a *persistable URI permission*
on Android (a StorageKit contract, taken explicitly on every result), so
grants survive restarts until revoked. `FileDialog::releaseGrant(url)` drops
one explicitly.

## QML

```qml
import StorageKit

Button { text: "Choose folder"; onClicked: Storage.pickFolder() }

Connections {
    target: Storage
    function onFolderPicked(url) { model.rootUrl = url }
}

ListView {
    model: FolderModel { id: model }   // roles: name, type, size, mime
}
```

- `Storage` (singleton): `pickFolder()`, `pickFile()`, `pickFiles()`,
  `pickSaveFile()`, `isGrantValid(url)`, small-file helpers
  `readAll(rootUrl, path)` / `writeAll(rootUrl, path, text)`. Results arrive
  via signals (`folderPicked`, `fileRead`, `fileWritten`, …).
- `FolderModel`: list model over one directory of a granted root
  (`rootUrl` + `path`), with `openItem`, `cdUp`, `newFile`, `newFolder`,
  `renameItem`, `removeItem`, `busy`, `errorOccurred`. All storage work runs
  on a worker thread.

## Integration

Self-contained CMake target, usable via `add_subdirectory`, git submodule or
`FetchContent`:

```cmake
add_subdirectory(libs/storagekit)
target_link_libraries(app PRIVATE StorageKit::StorageKit StorageKitplugin)
```

Requirements: Qt 6.8+ (developed against 6.11), C++20. Android needs no
manifest permissions — SAF works without any. The pickers are
`QFileDialog`-based on every platform (Qt's QPA helper shows the native
dialog), so `Qt6::Widgets` is required and applications must run a
`QApplication` (not `QGuiApplication`).

## Platform notes and limitations

- The C++ API is **blocking**; call it from a worker thread
  (`QtConcurrent::run`, as the QML layer does). SAF calls are binder IPC.
- Errors: `File` reports through `errorString()` (it is a `QIODevice`);
  `Dir` returns `bool` like `QDir`.
- `entryInfoList` supports filter bits `Files`, `Dirs`, `Hidden`, `Readable`,
  `Writable` and sort bits `Name`, `Time`, `Size`, `DirsFirst`, `DirsLast`,
  `Reversed`, `IgnoreCase`; other bits are ignored.
- Android: duplicate display names in one SAF directory are legal; resolution
  takes the first match. Some providers hand out non-seekable streams
  (`seek()` fails). Creating a file needs a MIME type: guessed from the
  suffix via `QMimeDatabase`, defaulting to `application/octet-stream`.
- Unix notions with no SAF equivalent (symlinks, permissions bits, locking)
  are not part of the API.

Deferred by design: iOS backend, change notifications, atomic-save helper,
cross-root copy with progress, MediaStore integration.
