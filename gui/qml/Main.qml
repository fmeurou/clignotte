import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts

ApplicationWindow {
    id: root
    visible: true
    width: 860
    height: 720
    title: "clignotte — " + (db.currentNotebookTitle.length ? db.currentNotebookTitle : "—")

    Material.theme: Material.Dark
    Material.accent: Material.Yellow

    property int editingNoteId: -1
    property bool editorOpen: false

    function openEditor(id) {
        editingNoteId = id
        editorOpen = true
    }

    function closeEditor() {
        editorOpen = false
        editingNoteId = -1
    }

    header: ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 4

            Label {
                text: "clignotte"
                font.bold: true
                font.pixelSize: 18
                Layout.alignment: Qt.AlignVCenter
            }
            Label {
                text: "▸ " + (db.currentNotebookTitle.length ? db.currentNotebookTitle : "—")
                opacity: 0.7
                Layout.alignment: Qt.AlignVCenter
            }
            Item { Layout.fillWidth: true }
            CheckBox {
                id: showClosed
                text: "Show closed"
                checked: false
                onToggled: db.notes.includeClosed = checked
            }
            ToolButton {
                id: kebabBtn
                text: "⋮"
                font.pixelSize: 20
                onClicked: kebabMenu.open()

                Menu {
                    id: kebabMenu
                    y: parent.height

                    Repeater {
                        model: db.notebooks
                        MenuItem {
                            text: title
                            checkable: true
                            checked: isCurrent
                            onTriggered: db.selectNotebook(notebookId)
                        }
                    }

                    MenuSeparator {}

                    MenuItem {
                        text: "New notebook…"
                        onTriggered: newNotebookDialog.open()
                    }
                }
            }
        }
    }

    // Full-width content: list behind, editor slides over from the right
    Item {
        anchors.fill: parent
        clip: true

        NoteList {
            anchors.fill: parent
            selectedId: root.editingNoteId
            onNoteActivated: (id) => root.openEditor(id)
        }

        NoteEditor {
            id: editorLayer
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width
            x: root.editorOpen ? 0 : parent.width
            enabled: root.editorOpen
            noteId: root.editingNoteId
            onClosed: root.closeEditor()

            Behavior on x {
                NumberAnimation { duration: 220; easing.type: Easing.OutCubic }
            }
        }
    }

    // FAB — bottom left, hidden while editor is open
    RoundButton {
        z: 10
        anchors.left: parent.left
        anchors.bottom: parent.bottom
        anchors.leftMargin: 24
        anchors.bottomMargin: 24
        width: 56
        height: 56
        visible: !root.editorOpen
        Material.background: Material.Yellow
        Material.foreground: "#000000"
        font.pixelSize: 28
        text: "+"
        onClicked: root.openEditor(-1)
    }

    Dialog {
        id: newNotebookDialog
        title: "New notebook"
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        modal: true
        TextField {
            id: newNotebookField
            width: 300
            placeholderText: "Notebook title"
            onAccepted: newNotebookDialog.accept()
        }
        onAccepted: {
            if (newNotebookField.text.trim().length > 0) {
                db.createNotebook(newNotebookField.text.trim())
                newNotebookField.text = ""
            }
        }
        onRejected: newNotebookField.text = ""
    }
}
