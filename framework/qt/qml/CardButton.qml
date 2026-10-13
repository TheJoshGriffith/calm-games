import QtQuick
import QtQuick.Templates as T
import Calm

// A large rounded button with a title, optional subtitle and trailing content.
T.AbstractButton {
    id: control

    property string subtitle
    property color fill: Theme.raised
    default property alias trailing: trailingSlot.data

    implicitHeight: Math.max(64, content.implicitHeight + 28)
    Accessible.name: subtitle ? text + ", " + subtitle : text

    contentItem: Item {
        id: content
        implicitHeight: textColumn.implicitHeight

        Column {
            id: textColumn
            anchors.left: parent.left
            anchors.right: trailingSlot.left
            anchors.leftMargin: 20
            anchors.rightMargin: 12
            anchors.verticalCenter: parent.verticalCenter
            spacing: 2
            CalmText {
                width: parent.width
                text: control.text
                font.pixelSize: 18
                font.weight: Font.Bold
                elide: Text.ElideRight
            }
            CalmText {
                width: parent.width
                visible: control.subtitle.length > 0
                text: control.subtitle
                font.pixelSize: 13
                color: Theme.muted
                elide: Text.ElideRight
            }
        }

        Item {
            id: trailingSlot
            anchors.right: parent.right
            anchors.rightMargin: 20
            anchors.verticalCenter: parent.verticalCenter
            width: childrenRect.width
            height: childrenRect.height
        }
    }

    background: Rectangle {
        radius: Theme.radius
        color: control.fill
        opacity: control.down ? 0.85 : 1
        Behavior on opacity { NumberAnimation { duration: Theme.quick } }
    }
}
