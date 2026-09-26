# File browser/editor demo for StorageKit. Built only when StorageKit is the
# top-level project (see STORAGEKIT_BUILD_EXAMPLE in the parent CMakeLists).
find_package(Qt6 REQUIRED COMPONENTS Quick Widgets)

qt_add_executable(appStorageKitExample
    main.cpp
)

qt_add_qml_module(appStorageKitExample
    URI StorageKitExample
    VERSION 1.0
    QML_FILES
        Main.qml
)

set_target_properties(appStorageKitExample PROPERTIES
    MACOSX_BUNDLE_BUNDLE_VERSION 0.1
    MACOSX_BUNDLE_SHORT_VERSION_STRING 0.1
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

# StorageKit's pickers are QFileDialog-based; the app runs a QApplication.
target_link_libraries(appStorageKitExample
    PRIVATE Qt6::Quick Qt6::Widgets StorageKit::StorageKit StorageKitplugin
)
