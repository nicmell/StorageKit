cmake_minimum_required(VERSION 3.21)

project(StorageKit VERSION 0.1 LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    include(CTest)
endif()

option(STORAGEKIT_BUILD_EXAMPLE "Build the StorageKit example app"
       ${PROJECT_IS_TOP_LEVEL})

set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Qml Concurrent Widgets)

if(PROJECT_IS_TOP_LEVEL)
    qt_standard_project_setup(REQUIRES 6.8)
endif()

qt_add_library(StorageKit STATIC
    include/StorageKit/Dir.h
    include/StorageKit/File.h
    include/StorageKit/FileDialog.h
    include/StorageKit/FileInfo.h
    src/backend_p.h
    src/dir.cpp
    src/file.cpp
    src/filedialog.cpp
    src/fileinfo.cpp
    src/fileinfo_p.h
    src/internal_p.h
)
add_library(StorageKit::StorageKit ALIAS StorageKit)

target_include_directories(StorageKit PUBLIC include PRIVATE src src/qml)
target_compile_features(StorageKit PUBLIC cxx_std_20)
target_link_libraries(StorageKit PUBLIC Qt6::Core PRIVATE Qt6::Qml Qt6::Concurrent Qt6::Widgets)

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

if(STORAGEKIT_BUILD_EXAMPLE)
    add_subdirectory(example)
endif()
