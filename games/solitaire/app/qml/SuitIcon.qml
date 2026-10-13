import QtQuick
import QtQuick.Shapes

// A filled suit symbol on a 24x24 grid: 0 spades, 1 hearts, 2 diamonds, 3 clubs.
Item {
    id: root

    property int suit: 0
    property color color: "black"
    property real size: 16

    readonly property var paths: [
        "M12 2C12 2 3 9.5 3 14a4 4 0 0 0 7.6 1.8L9.5 21h5l-1.1-5.2A4 4 0 0 0 21 14C21 9.5 12 2 12 2z",
        "M12 21C12 21 3 14.6 3 8.6A4.6 4.6 0 0 1 12 6.4A4.6 4.6 0 0 1 21 8.6C21 14.6 12 21 12 21z",
        "M12 2L20 12L12 22L4 12z",
        "M8 7.5a4 4 0 1 0 8 0a4 4 0 1 0-8 0zM3 13.5a4 4 0 1 0 8 0a4 4 0 1 0-8 0zM13 13.5a4 4 0 1 0 8 0a4 4 0 1 0-8 0zM11 12h2l1.5 9h-5z"
    ]

    implicitWidth: size
    implicitHeight: size

    Shape {
        anchors.fill: parent

        ShapePath {
            scale: Qt.size(root.size / 24, root.size / 24)
            fillColor: root.color
            fillRule: ShapePath.WindingFill
            strokeColor: "transparent"
            strokeWidth: 0
            PathSvg { path: root.paths[Math.max(0, Math.min(3, root.suit))] }
        }
    }
}
