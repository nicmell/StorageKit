# StorageKit — shared-storage filesystem library for desktop and Android.
# Self-contained: usable via add_subdirectory, git submodule or FetchContent.
cmake_minimum_required(VERSION 3.21)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(StorageKit VERSION 0.1 LANGUAGES CXX)
endif()

set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Qml Concurrent)

# Desktop pickers need QFileDialog (QtWidgets). Turn OFF for widget-free
# builds; pickers then resolve to empty results.
option(STORAGEKIT_WIDGET_DIALOGS "Use QFileDialog for pickers on desktop" ON)
if(ANDROID)
    set(STORAGEKIT_WIDGET_DIALOGS OFF)
endif()

qt_add_library(StorageKit STATIC
    include/StorageKit/FileInfo.h
    include/StorageKit/FileSystem.h
    src/fileinfo.cpp
    src/fileinfo_p.h
    src/filesystem.cpp
    src/filesystem_p.h
)
add_library(StorageKit::StorageKit ALIAS StorageKit)

target_include_directories(StorageKit PUBLIC include PRIVATE src src/qml)
target_compile_features(StorageKit PUBLIC cxx_std_20)
target_link_libraries(StorageKit PUBLIC Qt6::Core PRIVATE Qt6::Concurrent)

if(ANDROID)
    find_package(Qt6 REQUIRED COMPONENTS CorePrivate)
    target_sources(StorageKit PRIVATE
        src/android/jniutils.h
        src/android/jniutils.cpp
        src/android/safbackend.cpp
    )
    target_link_libraries(StorageKit PRIVATE Qt6::CorePrivate)
else()
    target_sources(StorageKit PRIVATE src/local/localbackend.cpp)
    if(STORAGEKIT_WIDGET_DIALOGS)
        find_package(Qt6 REQUIRED COMPONENTS Widgets)
        target_link_libraries(StorageKit PRIVATE Qt6::Widgets)
        # PUBLIC: the app checks this to choose QApplication over QGuiApplication.
        target_compile_definitions(StorageKit PUBLIC STORAGEKIT_WIDGET_DIALOGS)
    endif()
endif()

qt_add_qml_module(StorageKit
    URI StorageKit
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/StorageKit
    SOURCES
        src/qml/qmlfilesystem.h
        src/qml/qmlfilesystem.cpp
        src/qml/storagesingleton.h
        src/qml/storagesingleton.cpp
        src/qml/foldermodel.h
        src/qml/foldermodel.cpp
)
target_link_libraries(StorageKit PRIVATE Qt6::Qml)

if(NOT ANDROID AND BUILD_TESTING)
    add_subdirectory(tests)
endif()
