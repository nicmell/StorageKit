# CLAUDE.md

Qt 6 library for user-granted storage on desktop and Android. [README.md](README.md)
holds the API, the capability model, the build and test commands and the
requirements.

- `ctest` covers only the desktop backend (`src/local/`). Run changes under
  `src/android/` on a device or emulator, for example through the example app.
- SharedStorageExample (`libs/storagekit`) and LsdjToolkit
  (`common/dependency/storagekit`) pin this repo as a git submodule. After a
  change lands here, bump the submodule in each consumer that needs it.
