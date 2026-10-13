import QtQuick
import Calm

// A panel that rises from the bottom of the screen over a dimmed page, with a
// title and a close button. Make it the last child of a page (it fills its
// parent) and call open() / close(); a tap on the dimmed page closes it.
// Children stack in a column; give them `width: parent.width`.
Item {
    id: sheet

    property string title
    property bool opened: false
    default property alias content: body.data

    signal closed()

    function open() {
        opened = true
        forceActiveFocus()
    }
    function close() {
        if (!opened)
            return
        opened = false
        closed()
    }

    anchors.fill: parent
    z: 50
    visible: scrim.opacity > 0
    Keys.onEscapePressed: close()
    Keys.onBackPressed: (event) => {
        event.accepted = sheet.opened
        close()
    }

    Rectangle {
        id: scrim
        anchors.fill: parent
        color: Theme.scrim
        opacity: sheet.opened ? 1 : 0
        Behavior on opacity { NumberAnimation { duration: Theme.gentle } }

        MouseArea {
            anchors.fill: parent
            onClicked: sheet.close()
        }
    }

    Rectangle {
        id: panel
        // Wider screens get a sheet of phone width, centred.
        width: Math.min(parent.width, 560)
        anchors.horizontalCenter: parent.horizontalCenter
        // Extends one radius below the screen so only the top corners round.
        height: layout.implicitHeight + 20 + 32 + radius
        y: sheet.opened ? parent.height - height + radius : parent.height
        radius: Theme.radius + 8
        color: Theme.ground
        Behavior on y { NumberAnimation { duration: Theme.gentle; easing.type: Easing.OutCubic } }

        // Taps on the panel stay on the panel.
        MouseArea {
            anchors.fill: parent
        }

        Column {
            id: layout
            anchors.top: parent.top
            anchors.topMargin: 20
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: Theme.gutter + 4
            anchors.rightMargin: Theme.gutter + 4
            spacing: 16

            Item {
                width: parent.width
                height: Theme.touch

                CalmText {
                    anchors.left: parent.left
                    anchors.right: closeButton.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: sheet.title
                    font.pixelSize: 20
                    font.weight: Font.ExtraBold
                    elide: Text.ElideRight
                    Accessible.role: Accessible.Heading
                }
                IconButton {
                    id: closeButton
                    anchors.right: parent.right
                    anchors.rightMargin: -10
                    anchors.verticalCenter: parent.verticalCenter
                    iconPath: Icons.close
                    text: qsTr("Close")
                    onClicked: sheet.close()
                }
            }

            Column {
                id: body
                width: parent.width
                spacing: 12
            }
        }
    }
}
