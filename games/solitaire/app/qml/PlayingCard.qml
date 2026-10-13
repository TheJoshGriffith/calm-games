import QtQuick
import Calm

// One card's face or back. Turning over plays a short flip, unless the
// player asked for reduced motion.
Item {
    id: card

    property int rank: 1
    property int suit: 0
    property bool red: false
    property bool faceUp: false
    property bool hinted: false
    // Sideways offset for shaking, separate from x so it isn't animated.
    property real nudge: 0

    readonly property string rankText: ["A", "2", "3", "4", "5", "6", "7", "8", "9", "10", "J", "Q", "K"][Math.max(0, Math.min(12, rank - 1))]
    readonly property color ink: red ? Theme.danger : Theme.ink
    readonly property real cornerRadius: Math.max(4, width * 0.12)

    // 0 = showing the back, 1 = showing the face; the card is edge-on at 0.5.
    property real turn: faceUp ? 1 : 0
    Behavior on turn { NumberAnimation { duration: Theme.gentle * 0.8; easing.type: Easing.InOutQuad } }

    transform: [
        Scale {
            origin.x: card.width / 2
            xScale: Math.max(0.02, Math.abs(1 - 2 * card.turn))
        },
        Translate { x: card.nudge }
    ]

    Accessible.role: Accessible.StaticText
    Accessible.name: faceUp ? rankText + " " + [qsTr("of spades"), qsTr("of hearts"), qsTr("of diamonds"), qsTr("of clubs")][suit] : qsTr("Face-down card")

    // Face
    Rectangle {
        anchors.fill: parent
        visible: card.turn >= 0.5
        radius: card.cornerRadius
        color: Theme.raised
        border.width: card.hinted ? 2 : 1
        border.color: card.hinted ? Theme.accent : Theme.line

        layer.enabled: true
        layer.samples: 4

        CalmText {
            id: rankLabel
            x: card.width * 0.1
            y: card.width * 0.03
            text: card.rankText
            color: card.ink
            font.pixelSize: Math.round(card.width * 0.36)
            font.weight: Font.ExtraBold
            font.letterSpacing: card.rank === 10 ? -1 : 0
        }
        SuitIcon {
            anchors.right: parent.right
            anchors.rightMargin: card.width * 0.1
            anchors.verticalCenter: rankLabel.verticalCenter
            suit: card.suit
            color: card.ink
            size: card.width * 0.26
        }
        SuitIcon {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.bottom
            anchors.bottomMargin: card.height * 0.1
            suit: card.suit
            color: card.ink
            size: card.width * 0.5
            opacity: 0.9
        }
    }

    // Back
    Rectangle {
        anchors.fill: parent
        visible: card.turn < 0.5
        radius: card.cornerRadius
        color: Theme.accent
        border.width: card.hinted ? 2 : 0
        border.color: Theme.ink

        Rectangle {
            anchors.fill: parent
            anchors.margins: Math.max(3, card.width * 0.09)
            radius: Math.max(2, card.cornerRadius - 3)
            color: "transparent"
            border.width: 1
            border.color: Theme.ground
            opacity: 0.45
        }
    }
}
