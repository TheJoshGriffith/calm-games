import QtQuick
import QtQuick.Templates as T
import Calm

// Icon over a short label, for a game's toolbar (Undo, Notes, Hint...).
T.AbstractButton {
    id: control

    property string iconPath

    implicitWidth: 72
    implicitHeight: 60
    Accessible.name: text

    // The column is centred in the button, so the pressed or checked
    // background sits evenly around the icon and label.
    contentItem: Item {
        implicitWidth: content.implicitWidth
        implicitHeight: content.implicitHeight

        Column {
            id: content
            anchors.centerIn: parent
            spacing: 4
            Icon {
                anchors.horizontalCenter: parent.horizontalCenter
                path: control.iconPath
                opacity: control.enabled ? 1 : 0.35
            }
            CalmText {
                anchors.horizontalCenter: parent.horizontalCenter
                text: control.text
                font.pixelSize: 12
                font.weight: Font.DemiBold
                opacity: control.enabled ? 1 : 0.35
            }
        }
    }

    background: Rectangle {
        radius: Theme.radiusSmall
        color: control.checked ? Theme.selection : control.down ? Theme.subtle : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.quick } }
    }
}
