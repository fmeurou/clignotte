import QtQuick
import QtQuick.Controls
import QtQuick.Controls.Material
import QtQuick.Layouts
import QtQuick.Dialogs

Pane {
    id: root
    padding: 0

    property int noteId: -1
    signal closed()

    property var current: findRow()
    property var attachmentList: []

    // Draft state for new notes (noteId < 0)
    property string draftDueDate: ""
    property var draftTags: []
    property var draftAttachments: []

    function findRow() {
        if (noteId < 0) return null
        let m = db.notes
        for (let i = 0; i < m.rowCount(); ++i) {
            let idx = m.index(i, 0)
            if (m.data(idx, 0x101 /* IdRole */) === noteId) {
                return {
                    text:          m.data(idx, 0x102),
                    dueDate:       m.data(idx, 0x103),
                    doneAt:        m.data(idx, 0x104),
                    isDone:        m.data(idx, 0x105),
                    notebookTitle: m.data(idx, 0x109),
                    tags:          m.data(idx, 0x10A) || [],
                }
            }
        }
        return null
    }

    function reloadAttachments() {
        attachmentList = noteId >= 0 ? db.noteAttachments(noteId) : []
    }

    Connections {
        target: db
        function onNotesChanged() {
            current = findRow()
            if (current && !noteArea.activeFocus) noteArea.text = current.text
            root.reloadAttachments()
        }
        function onAttachmentsChanged() {
            root.reloadAttachments()
        }
    }

    onNoteIdChanged: {
        current = findRow()
        if (noteId >= 0) {
            noteArea.text = current ? current.text : ""
            reloadAttachments()
        } else {
            noteArea.text = ""
            draftDueDate = ""
            draftTags = []
            draftAttachments = []
            noteArea.forceActiveFocus()
        }
    }

    FileDialog {
        id: filePicker
        title: "Attach file"
        onAccepted: {
            let path = selectedFiles[0].toString().replace(/^file:\/\//, "")
            if (root.noteId < 0) {
                root.draftAttachments = root.draftAttachments.concat([path])
            } else {
                db.attachFile(root.noteId, path)
            }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 12
        spacing: 8

        // ── Header ────────────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true

            ToolButton {
                text: root.noteId < 0 ? "✕" : "←"
                font.pixelSize: 18
                onClicked: {
                    if (root.noteId >= 0 && current && noteArea.text !== current.text)
                        db.updateNoteText(root.noteId, noteArea.text)
                    noteArea.text = ""
                    root.closed()
                }
            }

            Label {
                text: root.noteId < 0
                      ? "New note  ·  " + db.currentNotebookTitle
                      : "Note #" + root.noteId + (current ? "  ·  " + current.notebookTitle : "")
                font.bold: true
                font.pixelSize: 15
                opacity: 0.9
                Layout.fillWidth: true
            }

            Button {
                visible: root.noteId >= 0 && current !== null
                text: current && current.isDone ? "Reopen" : "Mark done"
                flat: true
                onClicked: {
                    if (current.isDone) db.reopenNote(root.noteId)
                    else db.markDone(root.noteId)
                }
            }

            Button {
                visible: root.noteId >= 0
                text: "Delete"
                flat: true
                Material.foreground: "#ff5252"
                onClicked: deleteConfirm.open()
            }

            Button {
                text: "Save"
                highlighted: root.noteId < 0
                onClicked: {
                    if (root.noteId < 0) {
                        let t = noteArea.text.trim()
                        if (t.length > 0) {
                            let newId = db.addNote(t)
                            if (newId >= 0) {
                                if (draftDueDate.length > 0)
                                    db.setDueDate(newId, draftDueDate)
                                for (let tag of draftTags)
                                    db.tagNote(newId, tag)
                                for (let path of draftAttachments)
                                    db.attachFile(newId, path)
                            }
                        }
                    } else if (current && noteArea.text !== current.text) {
                        db.updateNoteText(root.noteId, noteArea.text)
                    }
                    noteArea.text = ""
                    root.closed()
                }
            }
        }

        // ── Text area ─────────────────────────────────────────────────────────
        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            TextArea {
                id: noteArea
                wrapMode: TextEdit.WordWrap
                placeholderText: root.noteId < 0 ? "Write your note…" : "Note text…"
                font.pixelSize: 15
                font.family: root.noteId >= 0 ? "monospace" : font.family
                onEditingFinished: {
                    if (root.noteId >= 0 && current && text !== current.text)
                        db.updateNoteText(root.noteId, text)
                }
            }
        }

        // ── Due date ──────────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Due:" }
            TextField {
                id: dueField
                placeholderText: "yyyy-MM-dd"
                text: root.noteId < 0
                      ? root.draftDueDate
                      : (current && current.dueDate
                         ? Qt.formatDate(new Date(current.dueDate), "yyyy-MM-dd") : "")
                Layout.preferredWidth: 140
                onEditingFinished: {
                    let v = text.trim()
                    if (root.noteId < 0) {
                        root.draftDueDate = /^\d{4}-\d{2}-\d{2}$/.test(v) ? v : ""
                    } else {
                        if (v.length === 0) db.clearDueDate(root.noteId)
                        else if (/^\d{4}-\d{2}-\d{2}$/.test(v)) db.setDueDate(root.noteId, v)
                    }
                }
            }
            Button {
                text: "Today"
                flat: true
                onClicked: {
                    let d = Qt.formatDate(new Date(), "yyyy-MM-dd")
                    dueField.text = d
                    if (root.noteId < 0) root.draftDueDate = d
                    else db.setDueDate(root.noteId, d)
                }
            }
            Button {
                text: "Clear"
                flat: true
                onClicked: {
                    dueField.text = ""
                    if (root.noteId < 0) root.draftDueDate = ""
                    else db.clearDueDate(root.noteId)
                }
            }
            Item { Layout.fillWidth: true }
        }

        // ── Tags ──────────────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Tags:" }
            Flow {
                Layout.fillWidth: true
                spacing: 4
                Repeater {
                    model: root.noteId < 0 ? root.draftTags : (current ? current.tags : [])
                    delegate: Rectangle {
                        radius: 10
                        color: "#33ffeb3b"
                        border.color: Material.accent
                        height: 22
                        width: tagChipLabel.implicitWidth + 28
                        Label {
                            id: tagChipLabel
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: "#" + modelData
                            font.pixelSize: 11
                        }
                        ToolButton {
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: 20; height: 20
                            text: "×"
                            onClicked: {
                                if (root.noteId < 0)
                                    root.draftTags = root.draftTags.filter(t => t !== modelData)
                                else
                                    db.untagNote(root.noteId, modelData)
                            }
                        }
                    }
                }
            }
            TextField {
                id: newTag
                placeholderText: "+tag"
                Layout.preferredWidth: 100
                onAccepted: {
                    let t = text.trim()
                    if (t.length > 0) {
                        if (root.noteId < 0) {
                            if (!root.draftTags.includes(t))
                                root.draftTags = root.draftTags.concat([t])
                        } else {
                            db.tagNote(root.noteId, t)
                        }
                        text = ""
                    }
                }
            }
        }

        // ── Attachments ───────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            Label { text: "Files:" }
            Flow {
                Layout.fillWidth: true
                spacing: 4
                Repeater {
                    model: root.noteId < 0 ? root.draftAttachments : root.attachmentList
                    delegate: Rectangle {
                        radius: 4
                        color: "#22ffffff"
                        height: 22
                        width: Math.min(attachChipLabel.implicitWidth + 28, 200)

                        property string rawPath: root.noteId < 0 ? modelData : (modelData.path || "")
                        property bool isUrl: rawPath.match(/^https?:\/\//)

                        property string displayName: {
                            if (isUrl) return rawPath.replace(/^https?:\/\//, "").split("/")[0]
                            let p = root.noteId < 0 ? rawPath : (modelData.originalPath || rawPath)
                            return p.split("/").pop()
                        }

                        MouseArea {
                            anchors.left: parent.left
                            anchors.right: removeBtn.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                let p = parent.rawPath
                                if (p.length > 0)
                                    Qt.openUrlExternally(parent.isUrl ? p : "file://" + p)
                            }
                        }
                        Label {
                            id: attachChipLabel
                            anchors.left: parent.left
                            anchors.right: removeBtn.left
                            anchors.leftMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            text: (parent.isUrl ? "🔗 " : "📎 ") + parent.displayName
                            font.pixelSize: 11
                            elide: Text.ElideMiddle
                        }
                        ToolButton {
                            id: removeBtn
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            width: 20; height: 20
                            text: "×"
                            onClicked: {
                                if (root.noteId < 0)
                                    root.draftAttachments = root.draftAttachments.filter(p => p !== modelData)
                                else
                                    db.detachFile(modelData.id)
                            }
                        }
                    }
                }
            }
            TextField {
                id: urlField
                placeholderText: "https://…"
                Layout.preferredWidth: 160
                inputMethodHints: Qt.ImhUrlCharactersOnly
                onAccepted: {
                    let u = text.trim()
                    if (u.length > 0) {
                        if (root.noteId < 0)
                            root.draftAttachments = root.draftAttachments.concat([u])
                        else
                            db.attachFile(root.noteId, u)
                        text = ""
                    }
                }
            }
            ToolButton {
                text: "📎"
                font.pixelSize: 16
                onClicked: filePicker.open()
            }
        }

        // ── Move to (edit mode only) ──────────────────────────────────────────
        RowLayout {
            visible: root.noteId >= 0
            Layout.fillWidth: true
            Label { text: "Move to:" }
            ComboBox {
                id: moveCombo
                Layout.preferredWidth: 200
                textRole: "title"
                valueRole: "title"
                model: db.notebooks
                onActivated: {
                    let t = currentValue
                    if (t && current && t !== current.notebookTitle)
                        db.moveNote(root.noteId, t)
                }
            }
            Item { Layout.fillWidth: true }
        }
    }

    Dialog {
        id: deleteConfirm
        title: "Delete note?"
        anchors.centerIn: parent
        standardButtons: Dialog.Yes | Dialog.No
        modal: true
        Label { text: "This cannot be undone." }
        onAccepted: {
            db.deleteNote(root.noteId)
            root.closed()
        }
    }
}
