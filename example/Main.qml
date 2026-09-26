import QtCore
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls.Basic
import StorageKit

ApplicationWindow {
    id: window
    width: 800
    height: 520
    minimumWidth: 400
    minimumHeight: 300
    visible: true
    title: qsTr("StorageKit Example")

    property url rootUrl
    property string currentFile: ""

    Settings {
        id: settings
        property string rootUrl
    }

    Component.onCompleted: {
        if (settings.rootUrl.length > 0 && Storage.isGrantValid(settings.rootUrl))
            window.rootUrl = settings.rootUrl
    }

    Connections {
        target: Storage
        function onFolderPicked(url) {
            window.rootUrl = url
            settings.rootUrl = url
            window.currentFile = ""
            editor.text = ""
        }
        function onFileRead(path, text, ok) {
            if (ok) {
                window.currentFile = path
                editor.text = text
            } else {
                statusLabel.flash(qsTr("Could not read %1").arg(path))
            }
        }
        function onFileWritten(path, ok) {
            statusLabel.flash(ok ? qsTr("Saved %1").arg(path)
                                 : qsTr("Could not save %1").arg(path))
            if (ok)
                folderModel.refresh()
        }
        function onSaveFilePicked(url) {
            statusLabel.flash(qsTr("Created %1").arg(url))
            folderModel.refresh()
        }
    }

    FolderModel {
        id: folderModel
        rootUrl: window.rootUrl
        onErrorOccurred: message => statusLabel.flash(message)
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            spacing: 4

            ToolButton {
                text: qsTr("Pick folder…")
                onClicked: Storage.pickFolder()
            }
            ToolButton {
                text: qsTr("Up")
                enabled: folderModel.canGoUp
                onClicked: folderModel.cdUp()
            }
            ToolButton {
                text: qsTr("New file")
                enabled: window.rootUrl != ""
                onClicked: Storage.pickSaveFile("untitled.txt", "text/plain")
            }
            Label {
                Layout.fillWidth: true
                elide: Text.ElideLeft
                text: window.rootUrl == ""
                      ? qsTr("No folder granted — pick one")
                      : "/" + folderModel.path
            }
            BusyIndicator {
                running: folderModel.busy
                implicitHeight: 24
                implicitWidth: 24
            }
        }
    }

    footer: Frame {
        visible: statusLabel.text.length > 0
        padding: 4
        Label {
            id: statusLabel
            anchors.fill: parent
            elide: Text.ElideRight

            function flash(message) {
                text = message
                statusTimer.restart()
            }
            Timer {
                id: statusTimer
                interval: 4000
                onTriggered: statusLabel.text = ""
            }
        }
    }

    SplitView {
        anchors.fill: parent

        ListView {
            id: listView
            SplitView.preferredWidth: window.width * 0.45
            SplitView.minimumWidth: 200
            clip: true

            model: folderModel
            delegate: ItemDelegate {
                required property int index
                required property string name
                required property string type
                required property double size
                required property string mime

                width: listView.width
                contentItem: RowLayout {
                    spacing: 8
                    Label { text: type === "dir" ? "📁" : "📄" }
                    Label {
                        Layout.fillWidth: true
                        elide: Text.ElideMiddle
                        text: name
                    }
                    Label {
                        opacity: 0.6
                        font.pixelSize: 11
                        text: type === "dir" ? "" : mime + " · " + size + " B"
                    }
                    ToolButton {
                        text: "✕"
                        onClicked: folderModel.removeItem(index)
                    }
                }
                onClicked: {
                    if (type === "dir")
                        folderModel.openItem(index)
                    else
                        Storage.readAll(window.rootUrl, folderModel.itemPath(index))
                }
            }

            Label {
                anchors.centerIn: parent
                visible: listView.count === 0 && !folderModel.busy
                opacity: 0.5
                text: window.rootUrl == "" ? qsTr("Pick a folder to browse it")
                                         : qsTr("Empty folder")
            }
        }

        ColumnLayout {
            SplitView.fillWidth: true
            spacing: 0

            RowLayout {
                Layout.margins: 4
                Label {
                    Layout.fillWidth: true
                    elide: Text.ElideLeft
                    text: window.currentFile.length > 0 ? window.currentFile
                                                        : qsTr("No file open")
                }
                Button {
                    text: qsTr("Save")
                    enabled: window.currentFile.length > 0
                    onClicked: Storage.writeAll(window.rootUrl, window.currentFile, editor.text)
                }
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.fillHeight: true
                TextArea {
                    id: editor
                    enabled: window.currentFile.length > 0
                    wrapMode: TextArea.Wrap
                    font.family: "monospace"
                }
            }
        }
    }
}
