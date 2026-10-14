import QtQuick
import Calm
import Mahjong

// The table: every tile of the layout, stacked in layers. Higher layers sit
// up and to the left, and each tile shows its thickness along its bottom and
// right edges. Tap a free tile to pick it, then its match to take the pair.
Item {
    id: board

    required property MahjongGame game

    readonly property var tileSlots: game.tileSlots
    readonly property var kinds: game.kinds
    readonly property var present: game.present
    readonly property var free: game.free
    readonly property var hintPair: game.hintPair

    TilePalette {
        id: tilePalette
        theme: board.game.tileTheme
    }

    // Tile size: the whole layout (plus the layers' offsets) fits the space.
    readonly property real depthRatio: 0.11
    readonly property real aspect: 1.3
    readonly property real tileWidth: {
        const across = game.boardWidth / 2 + (game.boardLayers + 1) * depthRatio
        const down = game.boardHeight / 2 * aspect + (game.boardLayers + 1) * depthRatio
        return Math.max(0, Math.min(width / across, height / down))
    }
    readonly property real tileHeight: tileWidth * aspect
    readonly property real depth: tileWidth * depthRatio
    readonly property real gap: Math.max(1, tileWidth * 0.04)
    readonly property real contentWidth: (game.boardWidth / 2 + (game.boardLayers + 1) * depthRatio) * tileWidth
    readonly property real contentHeight: (game.boardHeight / 2 * aspect + (game.boardLayers + 1) * depthRatio) * tileWidth

    // The tile on top at a point on the table, or -1. One handler for the
    // whole table, since tiles overlap their neighbours.
    function tileAt(px, py) {
        let best = -1
        let bestOrder = -1
        for (let i = 0; i < tileSlots.length; ++i) {
            if (!present[i])
                continue
            const s = tileSlots[i]
            const x = s[0] / 2 * tileWidth + (game.boardLayers - s[2]) * depth
            const y = s[1] / 2 * tileHeight + (game.boardLayers - s[2]) * depth
            const order = s[2] * 10000 + s[1] * 100 + s[0]
            if (px >= x && px < x + tileWidth && py >= y && py < y + tileHeight && order > bestOrder) {
                best = i
                bestOrder = order
            }
        }
        return best
    }

    Item {
        id: table
        width: board.contentWidth
        height: board.contentHeight
        anchors.centerIn: parent

        Repeater {
            model: board.tileSlots.length

            Item {
                id: tile

                required property int index
                readonly property var slot: board.tileSlots[index] || [0, 0, 0]
                readonly property int kind: index < board.kinds.length ? board.kinds[index] : -1
                readonly property bool onTable: index < board.present.length && board.present[index]
                readonly property bool isFree: index < board.free.length && board.free[index]
                readonly property bool selected: board.game.selected === index
                readonly property bool hinted: board.hintPair.length === 2
                                               && (board.hintPair[0] === index || board.hintPair[1] === index)

                // Higher layers draw over lower ones; within a layer, tiles
                // further right and down draw over their neighbours' edges.
                z: slot[2] * 10000 + slot[1] * 100 + slot[0]
                x: slot[0] / 2 * board.tileWidth + (board.game.boardLayers - slot[2]) * board.depth
                y: slot[1] / 2 * board.tileHeight + (board.game.boardLayers - slot[2]) * board.depth
                width: board.tileWidth
                height: board.tileHeight

                opacity: onTable ? 1 : 0
                visible: opacity > 0
                scale: onTable ? 1 : 1.12
                Behavior on opacity { NumberAnimation { duration: Theme.gentle; easing.type: Easing.OutQuad } }
                Behavior on scale { NumberAnimation { duration: Theme.gentle; easing.type: Easing.OutQuad } }

                Accessible.role: Accessible.Button
                Accessible.name: board.game.kindName(kind)
                                 + (selected ? qsTr(", selected") : isFree ? "" : qsTr(", blocked"))
                Accessible.ignored: !onTable

                transform: Translate { id: shake }

                // The tile's thickness.
                Rectangle {
                    x: board.depth * 0.8
                    y: board.depth * 0.8
                    width: parent.width - board.gap
                    height: parent.height - board.gap
                    radius: Math.min(Theme.radiusSmall, board.tileWidth * 0.16)
                    color: tilePalette.side
                    border.width: 1
                    border.color: tilePalette.edge
                }

                Rectangle {
                    id: body
                    width: parent.width - board.gap
                    height: parent.height - board.gap
                    radius: Math.min(Theme.radiusSmall, board.tileWidth * 0.16)
                    color: tile.selected ? Qt.tint(tilePalette.face, Qt.alpha(Theme.accent, 0.28)) : tilePalette.face
                    border.width: tile.selected || tile.hinted ? Math.max(2, board.tileWidth * 0.07) : 1
                    border.color: tile.selected || tile.hinted ? Theme.accent : tilePalette.edge
                    Behavior on color { ColorAnimation { duration: Theme.quick } }

                    TileFace {
                        anchors.fill: parent
                        anchors.margins: board.tileWidth * 0.12
                        kind: tile.kind
                        colours: tilePalette
                    }

                    // Blocked tiles rest in a little shade, so free ones stand out.
                    Rectangle {
                        anchors.fill: parent
                        radius: parent.radius
                        color: Theme.dark ? Theme.ground : Theme.ink
                        opacity: board.game.dimBlocked && !tile.isFree ? (Theme.dark ? 0.45 : 0.14) : 0
                        Behavior on opacity { NumberAnimation { duration: Theme.gentle } }
                    }

                    // A hinted pair breathes gently until it's taken.
                    SequentialAnimation on opacity {
                        running: tile.hinted && Theme.slow > 0
                        loops: Animation.Infinite
                        alwaysRunToEnd: true
                        NumberAnimation { to: 0.7; duration: Theme.slow; easing.type: Easing.InOutSine }
                        NumberAnimation { to: 1; duration: Theme.slow; easing.type: Easing.InOutSine }
                    }
                }

                // A blocked tile gives a small shake when tapped.
                SequentialAnimation {
                    id: shakeAnimation
                    NumberAnimation { target: shake; property: "x"; to: board.tileWidth * 0.06; duration: 50 }
                    NumberAnimation { target: shake; property: "x"; to: -board.tileWidth * 0.06; duration: 80 }
                    NumberAnimation { target: shake; property: "x"; to: 0; duration: 50 }
                }
                Connections {
                    target: board.game
                    function onBlockedTapped(t) {
                        if (t === tile.index && Theme.quick > 0)
                            shakeAnimation.restart()
                    }
                }
            }
        }

        TapHandler {
            onTapped: (eventPoint) => {
                const tile = board.tileAt(eventPoint.position.x, eventPoint.position.y)
                if (tile >= 0)
                    board.game.tap(tile)
            }
        }
    }
}
