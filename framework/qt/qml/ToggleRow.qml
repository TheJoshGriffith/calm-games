import QtQuick
import QtQuick.Templates as T
import Calm

// A settings row: label on the left, switch on the right. The whole row toggles.
T.AbstractButton {
    id: control

    checkable: true
    width: parent ? parent.width : implicitWidth
    implicitWidth: 320
    implicitHeight: 52
    Accessible.role: Accessible.CheckBox
    Accessible.name: text
    Accessible.checked: checked

    contentItem: Item {
        CalmText {
            anchors.left: parent.left
            anchors.right: track.left
            anchors.leftMargin: 16
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            text: control.text
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }

        Rectangle {
            id: track
            anchors.right: parent.right
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            width: 50
            height: 30
            radius: 15
            color: control.checked ? Theme.accent : Theme.trackOff
            Behavior on color { ColorAnimation { duration: Theme.quick } }

            Rectangle {
                width: 24
                height: 24
                radius: 12
                y: 3
                x: control.checked ? parent.width - width - 3 : 3
                color: control.checked ? Theme.knob : (Theme.dark ? Theme.muted : "#FFFFFF")
                Behavior on x { NumberAnimation { duration: Theme.quick; easing.type: Easing.OutCubic } }
            }
        }
    }
}
