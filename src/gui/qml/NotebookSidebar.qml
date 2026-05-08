import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Pane {
    padding: 0

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Label {
            text: "Notebooks"
            font.pixelSize: 14
            font.bold: true
            opacity: 0.7
            padding: 12
            Layout.fillWidth: true
        }

        ListView {
            id: list
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            model: db.notebooks

            delegate: ItemDelegate {
                width: list.width
                highlighted: isCurrent
                contentItem: RowLayout {
                    Label {
                        text: title
                        Layout.fillWidth: true
                        elide: Label.ElideRight
                        font.bold: isCurrent
                    }
                    Label {
                        text: isCurrent ? "●" : ""
                        color: Material.accent
                        opacity: 0.8
                    }
                }
                onClicked: db.selectNotebook(notebookId)
            }
        }
    }
}
