import QtQuick
import Calm

// What's printed on a tile, drawn from shapes so it looks the same on every
// platform (no fonts with mahjong glyphs needed):
//   Dots: coloured circles; Bamboo: sticks (a sprig for the 1); Characters:
//   the number over a red seal mark; Winds: E S W N; Dragons: a red seal, a
//   green sprout, a blue frame; Flowers and Seasons: small pictures.
// The "clear" theme shows every numbered tile as a big number with a little
// suit sign, for easy reading.
Item {
    id: face

    property int kind: -1
    required property TilePalette colours

    readonly property int suit: kind < 0 ? -1 : kind < 9 ? 0 : kind < 18 ? 1 : kind < 27 ? 2 : kind < 31 ? 3 : kind < 34 ? 4 : kind < 38 ? 5 : 6
    readonly property int rank: kind < 0 ? 0 : kind < 27 ? kind % 9 + 1 : kind < 31 ? kind - 26 : kind < 34 ? kind - 30 : kind < 38 ? kind - 33 : kind - 37
    readonly property bool numbered: suit >= 0 && suit <= 2
    readonly property bool pips: (suit === 0 || suit === 1) && !colours.plain
    readonly property real unit: Math.min(width, height)

    // Pip centres for 1-9, as fractions of the face.
    function pipSpots(n) {
        switch (n) {
        case 1: return [[0.5, 0.5]]
        case 2: return [[0.5, 0.28], [0.5, 0.72]]
        case 3: return [[0.27, 0.22], [0.5, 0.5], [0.73, 0.78]]
        case 4: return [[0.3, 0.27], [0.7, 0.27], [0.3, 0.73], [0.7, 0.73]]
        case 5: return [[0.28, 0.22], [0.72, 0.22], [0.5, 0.5], [0.28, 0.78], [0.72, 0.78]]
        case 6: return [[0.3, 0.2], [0.7, 0.2], [0.3, 0.5], [0.7, 0.5], [0.3, 0.8], [0.7, 0.8]]
        case 7: return [[0.22, 0.15], [0.5, 0.25], [0.78, 0.35], [0.3, 0.6], [0.7, 0.6], [0.3, 0.85], [0.7, 0.85]]
        case 8: return [[0.3, 0.14], [0.7, 0.14], [0.3, 0.38], [0.7, 0.38], [0.3, 0.62], [0.7, 0.62], [0.3, 0.86], [0.7, 0.86]]
        case 9: return [[0.2, 0.17], [0.5, 0.17], [0.8, 0.17], [0.2, 0.5], [0.5, 0.5], [0.8, 0.5], [0.2, 0.83], [0.5, 0.83], [0.8, 0.83]]
        }
        return []
    }
    function dotColor(i) {
        if (face.rank === 1)
            return face.colours.red
        if (face.rank === 2)
            return i === 0 ? face.colours.green : face.colours.blue
        const colours = [face.colours.blue, face.colours.green, face.colours.red]
        return colours[Math.floor(i / Math.max(1, Math.ceil(face.rank / 3))) % 3]
    }
    function stickColor(i) {
        // The middle stick of the odd bamboos is red, as on real tiles.
        return (face.rank % 2 === 1 && i === Math.floor(face.rank / 2) && face.rank > 1) ? face.colours.red : face.colours.green
    }
    function suitColour() {
        return face.suit === 0 ? face.colours.blue : face.suit === 1 ? face.colours.green : face.colours.red
    }

    readonly property var spots: pips && !(suit === 1 && rank === 1) ? pipSpots(rank) : []
    readonly property real pipSize: unit * (rank <= 1 ? 0.62 : rank <= 4 ? 0.34 : rank <= 6 ? 0.28 : 0.24)

    // Dots and bamboo sticks.
    Repeater {
        model: face.spots.length
        Rectangle {
            required property int index
            readonly property var spot: face.spots[index] || [0.5, 0.5]
            width: face.suit === 0 ? face.pipSize : face.unit * 0.13
            height: face.suit === 0 ? face.pipSize : Math.min(face.height * 0.2, face.pipSize * 1.05)
            x: spot[0] * face.width - width / 2
            y: spot[1] * face.height - height / 2
            radius: face.suit === 0 ? width / 2 : width / 2
            color: face.suit === 0 ? "transparent" : face.stickColor(index)
            border.width: face.suit === 0 ? Math.max(1.5, width * 0.2) : 0
            border.color: face.suit === 0 ? face.dotColor(index) : "transparent"

            // A dot's centre.
            Rectangle {
                visible: face.suit === 0
                anchors.centerIn: parent
                width: parent.width * 0.32
                height: width
                radius: width / 2
                color: face.dotColor(parent.index)
            }
        }
    }

    // 1 of Bamboo: a sprig instead of a single stick.
    Icon {
        visible: face.pips && face.suit === 1 && face.rank === 1
        anchors.centerIn: parent
        size: face.unit * 0.9
        strokeWidth: 2.4
        color: face.colours.green
        path: "M12 21V8M12 13c-4 0-6-3-6-7 4 0 6 3 6 7zM12 11c0-4 2-7 6-7 0 4-2 7-6 7"
    }

    // Characters (and every numbered tile in the clear theme): the number,
    // with a little mark below in the suit's colour.
    CalmText {
        visible: face.suit === 2 || (face.numbered && face.colours.plain)
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.verticalCenter: parent.verticalCenter
        anchors.verticalCenterOffset: -face.height * 0.1
        text: face.rank
        font.pixelSize: face.height * 0.52
        font.weight: Font.ExtraBold
        color: face.colours.plain ? face.suitColour() : face.colours.ink
    }
    Item {
        visible: face.suit === 2 || (face.numbered && face.colours.plain)
        anchors.horizontalCenter: parent.horizontalCenter
        y: face.height * 0.7
        width: face.unit * 0.6
        height: face.height * 0.2

        // Characters: a red seal bar. Clear theme: the suit's own sign.
        Rectangle {
            visible: face.suit === 2
            anchors.centerIn: parent
            width: parent.width
            height: Math.max(2, parent.height * 0.42)
            radius: height / 2
            color: face.colours.red
        }
        Row {
            visible: face.suit === 0
            anchors.centerIn: parent
            spacing: parent.width * 0.08
            Repeater {
                model: 3
                Rectangle {
                    width: face.unit * 0.14
                    height: width
                    radius: width / 2
                    color: face.colours.blue
                }
            }
        }
        Row {
            visible: face.suit === 1
            anchors.centerIn: parent
            spacing: parent.width * 0.1
            Repeater {
                model: 3
                Rectangle {
                    width: face.unit * 0.09
                    height: face.height * 0.18
                    radius: width / 2
                    color: face.colours.green
                }
            }
        }
    }

    // Winds.
    CalmText {
        visible: face.suit === 3
        anchors.centerIn: parent
        text: face.suit === 3 ? ["E", "S", "W", "N"][face.rank - 1] : ""
        font.pixelSize: face.height * 0.56
        font.weight: Font.ExtraBold
        color: face.colours.blue
    }

    // Red dragon: a seal; white dragon: a blue frame.
    Rectangle {
        visible: face.suit === 4 && face.rank !== 2
        anchors.centerIn: parent
        width: face.unit * 0.66
        height: face.rank === 3 ? face.height * 0.72 : width
        radius: face.unit * 0.08
        color: "transparent"
        border.width: Math.max(2, face.unit * 0.1)
        border.color: face.rank === 1 ? face.colours.red : face.colours.blue

        Rectangle {
            visible: face.rank === 1
            anchors.centerIn: parent
            width: Math.max(2, face.unit * 0.1)
            height: face.height * 0.62
            radius: width / 2
            color: face.colours.red
        }
        Rectangle {
            visible: face.rank === 3
            anchors.fill: parent
            anchors.margins: parent.border.width * 2
            radius: face.unit * 0.04
            color: "transparent"
            border.width: Math.max(1, face.unit * 0.04)
            border.color: face.colours.blue
        }
    }
    // Green dragon: a sprout.
    Icon {
        visible: face.suit === 4 && face.rank === 2
        anchors.centerIn: parent
        size: face.unit * 0.9
        strokeWidth: 2.6
        color: face.colours.green
        path: "M12 21v-9M12 12C12 7 9 4 4 4c0 5 3 8 8 8zM12 12c0-5 3-8 8-8 0 5-3 8-8 8"
    }

    // Flowers and seasons: a little picture and its number in the corner.
    Icon {
        visible: face.suit >= 5
        anchors.centerIn: parent
        anchors.verticalCenterOffset: face.height * 0.04
        size: face.unit * 0.92
        strokeWidth: 1.8
        color: face.suit === 5 ? face.colours.red
             : face.rank === 1 ? face.colours.green
             : face.rank === 2 ? face.colours.red
             : face.rank === 3 ? face.colours.red
             : face.colours.blue
        path: {
            if (face.suit < 5 || face.rank < 1)
                return ""
            if (face.suit === 5) {
                return [
                    "M9.8 12a2.2 2.2 0 1 0 4.4 0a2.2 2.2 0 1 0-4.4 0M9.1 6.8a2.9 2.9 0 1 0 5.8 0a2.9 2.9 0 1 0-5.8 0M14 10.4a2.9 2.9 0 1 0 5.8 0a2.9 2.9 0 1 0-5.8 0M12.2 16.2a2.9 2.9 0 1 0 5.8 0a2.9 2.9 0 1 0-5.8 0M6 16.2a2.9 2.9 0 1 0 5.8 0a2.9 2.9 0 1 0-5.8 0M4.2 10.4a2.9 2.9 0 1 0 5.8 0a2.9 2.9 0 1 0-5.8 0",
                    "M9.8 12a2.2 2.2 0 1 0 4.4 0a2.2 2.2 0 1 0-4.4 0M8.6 7a3.4 3.4 0 1 0 6.8 0a3.4 3.4 0 1 0-6.8 0M12.9 14.5a3.4 3.4 0 1 0 6.8 0a3.4 3.4 0 1 0-6.8 0M4.3 14.5a3.4 3.4 0 1 0 6.8 0a3.4 3.4 0 1 0-6.8 0",
                    "M9.8 12a2.2 2.2 0 1 0 4.4 0a2.2 2.2 0 1 0-4.4 0M15 12h5.5M14.6 13.5l4.8 2.7M13.5 14.6l2.7 4.8M12 15v5.5M10.5 14.6l-2.7 4.8M9.4 13.5l-4.8 2.7M9 12H3.5M9.4 10.5L4.6 7.7M10.5 9.4L7.7 4.6M12 9V3.5M13.5 9.4l2.7-4.8M14.6 10.5l4.8-2.8",
                    "M8 21c1-6 2-12 1-18M9 9c3-1 5-3 6-6M9 14c3 0 6-1 8-4M8.5 18.5c-2-1-3.5-3-4-5.5"
                ][face.rank - 1]
            }
            return [
                "M12 21v-9M12 12C12 7 9 4 4 4c0 5 3 8 8 8zM12 12c0-5 3-8 8-8 0 5-3 8-8 8",
                "M8 12a4 4 0 1 0 8 0a4 4 0 1 0-8 0M18.5 12H21M16.6 16.6l1.8 1.8M12 18.5V21M7.4 16.6l-1.8 1.8M5.5 12H3M7.4 7.4L5.6 5.6M12 5.5V3M16.6 7.4l1.8-1.8",
                "M5 19C5 10 10 5 19 5c0 9-5 14-14 14zM5 19l9-9",
                "M12 12l7.8 4.5M12 12v9M12 12l-7.8 4.5M12 12L4.2 7.5M12 12V3M12 12l7.8-4.5M16.8 16.9v-2.1l1.9-1.1M10.1 18.6l1.9-1.1 1.9 1.1M5.3 13.7l1.9 1.1v2.1M7.2 7v2.2l-1.9 1.1M13.9 5.4L12 6.5l-1.9-1.1M18.7 10.3l-1.9-1.1V7"
            ][face.rank - 1]
        }
    }
    CalmText {
        visible: face.suit >= 5
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: face.unit * 0.04
        text: face.rank
        font.pixelSize: face.height * 0.2
        font.weight: Font.ExtraBold
        color: face.colours.ink
        opacity: 0.7
    }
}
