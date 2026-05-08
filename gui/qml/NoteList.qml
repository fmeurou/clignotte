import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material

Item {
    id: root
    property int selectedId: -1
    signal noteActivated(int id)

    Connections {
        target: db
        function onCurrentNotebookChanged() { searchField.clear() }
    }

    TextField {
        id: searchField
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.margins: 8
        placeholderText: "Search…"
        Material.accent: Material.BlueGrey
        leftPadding: 10
        onTextChanged: db.notes.searchText = text
    }

    ListView {
        id: list
        anchors.top: searchField.bottom
        anchors.topMargin: 4
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        clip: true
        model: db.notes

        delegate: Item {
            id: del
            width: list.width
            height: 72

            property bool hovered: hoverHandler.hovered

            HoverHandler { id: hoverHandler }

            // Row background
            Rectangle {
                anchors.fill: parent
                color: noteId === root.selectedId
                    ? Qt.rgba(1, 1, 1, 0.10)
                    : del.hovered
                        ? Qt.rgba(1, 1, 1, 0.05)
                        : "transparent"
            }

            // Bottom divider
            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: "#18ffffff"
            }

            // Note content: two lines
            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                anchors.verticalCenter: parent.verticalCenter
                spacing: 5

                // Line 1: note text
                Text {
                    width: parent.width
                    text: {
                        let t = model.text
                        if (t.startsWith("!") || t.startsWith("*") || t.startsWith("~"))
                            t = t.substring(1).trimStart()
                        return t.split("\n")[0]
                    }
                    elide: Text.ElideRight
                    color: isDone ? "#777" : "#e0e0e0"
                    font.pixelSize: 14
                    font.strikeout: isDone
                }

                // Line 2: notebook chip · due date · attachments
                Row {
                    spacing: 8

                    Rectangle {
                        radius: 3
                        color: "#28ffffff"
                        height: 16
                        width: nbChipLabel.implicitWidth + 10
                        anchors.verticalCenter: parent.verticalCenter
                        Text {
                            id: nbChipLabel
                            anchors.centerIn: parent
                            text: notebookTitle
                            font.pixelSize: 10
                            color: "#cccccc"
                        }
                    }

                    Text {
                        visible: !!dueDate
                        text: dueDate ? ("📅 " + Qt.formatDate(new Date(dueDate), "yyyy-MM-dd")) : ""
                        font.pixelSize: 11
                        color: {
                            if (!dueDate) return "#888"
                            let today = new Date()
                            today.setHours(0, 0, 0, 0)
                            return new Date(dueDate) < today ? "#ff5252" : "#aaaaaa"
                        }
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        visible: attachmentCount > 0
                        text: "📎 " + attachmentCount
                        font.pixelSize: 11
                        color: "#aaaaaa"
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Repeater {
                        model: tags
                        Rectangle {
                            radius: 3
                            color: "#1a80d0ff"
                            height: 16
                            width: tagLabel.implicitWidth + 10
                            anchors.verticalCenter: parent.verticalCenter
                            Text {
                                id: tagLabel
                                anchors.centerIn: parent
                                text: modelData
                                font.pixelSize: 10
                                color: "#80d0ff"
                            }
                        }
                    }
                }
            }

            // Tick button — appears on hover for open notes
            RoundButton {
                visible: del.hovered && !isDone
                anchors.right: parent.right
                anchors.rightMargin: 12
                anchors.verticalCenter: parent.verticalCenter
                width: 36
                height: 36
                text: "✓"
                font.pixelSize: 14
                Material.background: "#4caf50"
                Material.foreground: "white"
                z: 2
                onClicked: db.markDone(noteId)
            }

            // Click target (below the tick button in z-order)
            MouseArea {
                anchors.fill: parent
                z: 1
                onClicked: root.noteActivated(noteId)
            }
        }

        Text {
            anchors.centerIn: parent
            visible: list.count === 0
            text: "No notes here yet"
            color: "#666"
        }
    }
}
