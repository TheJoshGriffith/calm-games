import QtQuick
import QtQuick.Templates as T
import Calm

// A round, icon-only button. Set `text` for screen readers.
T.AbstractButton {
    id: control

    property string iconPath
    property color color: Theme.ink

    implicitWidth: Theme.touch
    implicitHeight: Theme.touch
    Accessible.name: text

    contentItem: Item {
        Icon {
            anchors.centerIn: parent
            path: control.iconPath
            color: control.color
            opacity: control.enabled ? 1 : 0.35
        }
    }

    background: Rectangle {
        radius: width / 2
        color: control.down ? Theme.subtle : "transparent"
        Behavior on color { ColorAnimation { duration: Theme.quick } }
    }
}
