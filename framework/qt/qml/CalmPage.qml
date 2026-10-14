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
            id: titleColumn
            // Centred on the screen when it fits; a long title next to
            // several actions moves left (and then elides) rather than run
            // under them.
            readonly property real leftEdge: Theme.touch + 8
            readonly property real rightEdge: parent.width - Math.max(Theme.touch, actionSlot.width) - 8
            readonly property real wanted: Math.max(titleText.implicitWidth, subtitleText.implicitWidth)
            anchors.verticalCenter: backButton.verticalCenter
            width: Math.max(0, Math.min(wanted, rightEdge - leftEdge))
            x: Math.max(leftEdge, Math.min((parent.width - width) / 2, rightEdge - width))
            CalmText {
                id: titleText
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: page.title
                font.pixelSize: 17
                font.weight: Font.Bold
                elide: Text.ElideRight
            }
            CalmText {
                id: subtitleText
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
