import QtQuick
import Calm

// A titled, rounded group of settings rows with hairlines between them.
Column {
    id: root

    property string title
    default property alias rows: body.data

    spacing: 8

    CalmText {
        x: 4
        text: root.title.toUpperCase()
        font.pixelSize: 13
        font.weight: Font.Bold
        font.letterSpacing: 1
        color: Theme.muted
    }

    Rectangle {
        width: root.width
        height: body.implicitHeight
        radius: Theme.radius
        color: Theme.surface
        clip: true

        Column {
            id: body
            width: parent.width
        }

        // Hairlines between rows.
        Repeater {
            model: Math.max(0, body.children.length - 1)
            Rectangle {
                required property int index
                x: 16
                width: parent.width - 16
                height: 1
                color: Theme.line
                y: body.children[index].y + body.children[index].height
            }
        }
    }
}
