import QtQuick
import Calm

// A screen with the Calm background, a header (back button, title, optional
// subtitle and trailing actions) and a content area below it.
Item {
    id: page

    property string title
    property string subtitle
    property bool showBack: true
    property alias actions: actionSlot.data
    default property alias content: body.data
    readonly property int contentWidth: body.width

    signal backRequested()

    Rectangle {
        anchors.fill: parent
        color: Theme.ground
    }

    Item {
        id: header
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Theme.gutter
        anchors.rightMargin: Theme.gutter
        height: 44 + Theme.safeTop

        IconButton {
            id: backButton
            anchors.left: parent.left
            anchors.bottom: parent.bottom
            visible: page.showBack
            iconPath: Icons.back
            text: qsTr("Back")
            onClicked: page.backRequested()
        }

        Column {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.verticalCenter: backButton.verticalCenter
            width: parent.width - 2 * (Theme.touch + 8)
            CalmText {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: page.title
                font.pixelSize: 17
                font.weight: Font.Bold
                elide: Text.ElideRight
            }
            CalmText {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                visible: page.subtitle.length > 0
                text: page.subtitle
                font.pixelSize: 13
                color: Theme.muted
            }
        }

        Row {
            id: actionSlot
            anchors.right: parent.right
            anchors.bottom: parent.bottom
        }
    }

    Item {
        id: body
        anchors.top: header.bottom
        anchors.topMargin: 16
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: Theme.gutter
        anchors.rightMargin: Theme.gutter
        anchors.bottomMargin: 24
    }
}
