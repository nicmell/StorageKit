# StorageKit

A small Qt 6 library giving desktop and Android QML applications uniform access to
user-granted storage, with an interface as close to the unix filesystem API as
scoped storage honestly allows.

On Android, apps cannot browse the filesystem: access goes through the
[Storage Access Framework](https://developer.android.com/guide/topics/providers/document-provider)
(SAF) — the user picks a folder in a system dialog, the app receives an opaque
`content://` tree URI valid only under that grant. Qt 6 covers parts of this
(native picker, `content://` reads through `QFile`), but leaves gaps: no
persistable permissions, no create/rename/move/delete inside a picked tree, no
batched metadata. StorageKit fills those gaps and gives desktop the same API on
top of plain directories. It is a modernized take on
[KDAB's shared storage wrapper](https://www.kdab.com/android-shared-storage-qt-wrapper/)
(MIT, © 2022 Klarälvdalens Datakonsult AB), whose DocumentsContract JNI
patterns it reuses.

## Model: a capability filesystem

One `StorageKit::FileSystem` instance = one granted root:

- desktop: a directory (`file://` URL)
- Android: a SAF tree URI (`content://`), granted by the system folder picker

An instance can never reach outside its root — `..` and absolute paths fail
with `EACCES` on every platform. The constructor is synchronous and never shows
UI; the `pick*()` static factories are the only entry points that do. This
mirrors `openat(2)`-style capability designs (and matches what a SAF grant
actually is).

Persistence: store `fs.root()` (e.g. in `QSettings`) and rebuild with the
constructor next launch. The folder picker takes a *persistable URI permission*
on Android, so restored roots keep working across restarts until the user
revokes them. `FileSystem::releaseGrant(url)` drops a grant explicitly.

## Dual API, intentional

**I/O and mutation follow unix.** `open()` returns a *real native file
descriptor* (on Android via `ParcelFileDescriptor.detachFd()`), so
`read`/`write`/`lseek`/`fstat`/`close` are plain libc — the library ships no
I/O wrappers. Mutating calls return `0`/`-1` with `errno` set (`ENOENT`,
`EACCES`, `EEXIST`, `ENOTEMPTY`, `EISDIR`, `ENOTSUP`, …).

**Metadata and listing follow Qt.** `entryInfoList()` mirrors
`QDir::entryInfoList` and returns `StorageKit::FileInfo` objects (a
`QFileInfo`-compatible subset plus `mimeType()`), filled from **one batched
query per directory** — important on Android where every provider call is
binder IPC.

```cpp
#include <StorageKit/FileSystem.h>
#include <fcntl.h>
#include <unistd.h>

using namespace StorageKit;

// First run: the user grants a folder (SAF tree picker / QFileDialog).
FileSystem::pickFolder().then([](FileSystem fs) {
    if (!fs.isValid())
        return; // canceled
    settings.setValue("root", fs.root()); // persist the grant

    fs.mkdir("notes");
    int fd = fs.open("notes/todo.txt", O_WRONLY | O_CREAT | O_TRUNC);
    write(fd, "hello\n", 6);
    close(fd);

    for (const FileInfo &fi : fs.entryInfoList("notes", {"*.txt"}, QDir::Files, QDir::Name))
        qDebug() << fi.fileName() << fi.size() << fi.mimeType();
});

// Every later launch: silent, no UI.
FileSystem fs(settings.value("root").toUrl());
if (!fs.isValid())
    ; // grant revoked -> re-run pickFolder()
```

Single files can be picked too: `pickFile(mimeTypes)` / `pickFiles(mimeTypes)`
return `content://`/`file://` URLs (a file grant, not a tree), opened with
`FileSystem::openUrl(url, flags)`.

## QML

```qml
import StorageKit

Button { text: "Choose folder"; onClicked: Storage.pickFolder() }

Connections {
    target: Storage
    function onFolderPicked(fileSystem) { model.fileSystem = fileSystem }
}

ListView {
    model: FolderModel { id: model }   // roles: name, type, size, mime
}
```

- `Storage` (singleton): `pickFolder()`, `pickFile()`, `pickFiles()`,
  `restore(url)`, small-file helpers `readAll(fs, path)` / `writeAll(fs, path, text)`.
  Results arrive via signals (`folderPicked`, `fileRead`, `fileWritten`, …).
- `FileSystem`: QObject handle (`root`, `valid`), returned by the calls above.
- `FolderModel`: list model over one directory with `openItem`, `cdUp`,
  `newFile`, `newFolder`, `renameItem`, `removeItem`, `busy`, `errorOccurred`.
  All storage work runs on a worker thread.

## Integration

Self-contained CMake target, usable via `add_subdirectory`, git submodule or
`FetchContent`:

```cmake
add_subdirectory(libs/storagekit)
target_link_libraries(app PRIVATE StorageKit::StorageKit StorageKitplugin)
```

Requirements: Qt 6.8+ (developed against 6.11), C++20. Android needs no
manifest permissions — SAF works without any.

Options:

- `STORAGEKIT_WIDGET_DIALOGS` (default `ON` on desktop): desktop pickers use
  `QFileDialog`, linking `Qt6::Widgets`; the app should then use
  `QApplication` (the `STORAGEKIT_WIDGET_DIALOGS` compile definition is
  exported for exactly that `#ifdef`). With the option `OFF`, pickers resolve
  to empty results and the app must provide its own picker UI.

## Platform notes and limitations

- The core API is **blocking**; call it from a worker thread
  (`QtConcurrent::run`, as the QML layer does). SAF calls are binder IPC.
- `entryInfoList` supports filter bits `Files`, `Dirs`, `Hidden`, `Readable`,
  `Writable` and sort bits `Name`, `Time`, `Size`, `DirsFirst`, `DirsLast`,
  `Reversed`, `IgnoreCase`; other bits are ignored.
- Android: duplicate display names in one SAF directory are legal; path
  resolution takes the first match. Some providers hand out non-seekable
  pipes (`lseek` fails with `ESPIPE`). `O_APPEND` with `O_RDWR` is emulated
  (open + seek to end). `O_CREAT` needs a MIME type: guessed from the file
  suffix via `QMimeDatabase`, defaulting to `application/octet-stream`.
- Unsupported unix notions (symlinks, hardlinks, chmod, locking) fail with
  `ENOTSUP` rather than pretending.

Deferred by design: iOS backend (the API is shaped to allow one), change
notifications, atomic-save helper, cross-filesystem copy with progress,
MediaStore integration.
