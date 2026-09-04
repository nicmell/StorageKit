# StorageKit — shared-storage filesystem library for desktop and Android.
# Self-contained: usable via add_subdirectory, git submodule or FetchContent.
cmake_minimum_required(VERSION 3.21)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(StorageKit VERSION 0.1 LANGUAGES CXX)
endif()

set(CMAKE_AUTOMOC ON)

# Pickers are QFileDialog-based on every platform (the QPA helper shows the
# native dialog — the SAF pickers on Android), so Widgets is a hard
# dependency and apps must run a QApplication.
find_package(Qt6 REQUIRED COMPONENTS Core Qml Concurrent Widgets)

qt_add_library(StorageKit STATIC
    include/StorageKit/FileInfo.h
    include/StorageKit/FileSystem.h
    src/fileinfo.cpp
    src/fileinfo_p.h
    src/filesystem.cpp
    src/filesystem_p.h
    src/pickers.cpp
)
add_library(StorageKit::StorageKit ALIAS StorageKit)

target_include_directories(StorageKit PUBLIC include PRIVATE src src/qml)
target_compile_features(StorageKit PUBLIC cxx_std_20)
# Qml is public: the QML registration macros live in the public FileSystem.h.
target_link_libraries(StorageKit PUBLIC Qt6::Core Qt6::Qml PRIVATE Qt6::Concurrent Qt6::Widgets)

if(ANDROID)
    target_sources(StorageKit PRIVATE
        src/android/jniutils.h
        src/android/jniutils.cpp
        src/android/safbackend.cpp
    )
else()
    target_sources(StorageKit PRIVATE src/local/localbackend.cpp)
endif()

qt_add_qml_module(StorageKit
    URI StorageKit
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/StorageKit
    SOURCES
        src/qml/storage.h
        src/qml/storage.cpp
        src/qml/foldermodel.h
        src/qml/foldermodel.cpp
)
target_link_libraries(StorageKit PRIVATE Qt6::Qml)

if(NOT ANDROID AND BUILD_TESTING)
    add_subdirectory(tests)
endif()
