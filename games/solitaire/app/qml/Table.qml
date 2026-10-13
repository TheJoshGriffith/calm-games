import QtQuick
import Calm
import Solitaire

// The Klondike table: stock, waste and foundations along the top, seven
// columns below. Cards glide between places; tap a card to send it to its
// best spot, or drag it (and the cards on it) where you want it.
Item {
    id: table

    required property SolitaireGame game

    readonly property real gap: Math.max(4, Math.round(width * 0.016))
    readonly property real cardW: Math.floor((width - 6 * gap) / 7)
    readonly property real cardH: Math.round(cardW * 1.42)
    readonly property real tableauTop: cardH + gap * 3
    readonly property var hintCards: game.hint.cards || []

    // Cards being dragged and how far the pointer has moved them.
    property var dragGroup: []
    property bool dragging: false
    property point dragOffset: Qt.point(0, 0)

    readonly property var placement: layout(game.cards, width, height)

    function slotX(column) { return column * (cardW + gap) }

    // Where every card sits, indexed by card id.
    function layout(cards, w, h) {
        const down = [0, 0, 0, 0, 0, 0, 0]
        const total = [0, 0, 0, 0, 0, 0, 0]
        for (let i = 0; i < cards.length; ++i) {
            const c = cards[i]
            if (c.pile === SolitaireGame.Tableau) {
                total[c.slot]++
                if (!c.faceUp)
                    down[c.slot]++
            }
        }
        // Columns squeeze together when they would run off the table.
        const room = Math.max(0, h - tableauTop - cardH)
        const downSteps = []
        const upSteps = []
        for (let col = 0; col < 7; ++col) {
            let d = cardH * 0.12
            let u = cardH * 0.3
            const need = down[col] * d + Math.max(0, total[col] - down[col] - 1) * u
            if (need > room && need > 0) {
                d *= room / need
                u *= room / need
            }
            downSteps.push(d)
            upSteps.push(u)
        }

        const fan = cardW * 0.32
        const out = []
        for (let i = 0; i < cards.length; ++i) {
            const c = cards[i]
            let x = 0
            let y = 0
            if (c.pile === SolitaireGame.Waste) {
                x = slotX(1)
                if (game.drawCount > 1)
                    x += Math.max(0, c.pos - (c.count - 3)) * fan
            } else if (c.pile === SolitaireGame.Foundation) {
                x = slotX(3 + c.slot)
            } else if (c.pile === SolitaireGame.Tableau) {
                const d = down[c.slot]
                x = slotX(c.slot)
                y = tableauTop + Math.min(c.pos, d) * downSteps[c.slot] + Math.max(0, c.pos - d) * upSteps[c.slot]
            }
            out.push({ x: x, y: y })
        }
        return out
    }

    // Which pile a dragged card's centre is over: [pile, slot] or null.
    function pileAt(x, y) {
        const column = Math.max(0, Math.min(6, Math.floor((x + gap / 2) / (cardW + gap))))
        if (y < tableauTop - gap) {
            if (column >= 3)
                return [SolitaireGame.Foundation, column - 3]
            return null
        }
        return [SolitaireGame.Tableau, column]
    }

    function beginDrag(id) {
        const cards = game.cards
        const me = cards[id]
        const group = []
        for (let i = 0; i < cards.length; ++i) {
            const c = cards[i]
            if (c.pile === me.pile && c.slot === me.slot && c.pos >= me.pos)
                group.push(i)
        }
        dragOffset = Qt.point(0, 0)
        dragGroup = group
        dragging = true
    }

    function endDrag(id, drop) {
        const spot = placement[id]
        const target = drop ? pileAt(spot.x + dragOffset.x + cardW / 2, spot.y + dragOffset.y + cardH / 3) : null
        // Turn animations back on before the cards leave the pointer, so
        // they glide to wherever they end up.
        dragging = false
        dragGroup = []
        dragOffset = Qt.point(0, 0)
        if (target)
            game.dropCard(id, target[0], target[1])
    }

    // Empty places: stock, foundations and columns.
    Rectangle {
        id: stockSlot
        x: table.slotX(0)
        width: table.cardW
        height: table.cardH
        radius: Math.max(4, width * 0.12)
        color: "transparent"
        border.width: table.game.hint.stock === true ? 2 : 1
        border.color: table.game.hint.stock === true ? Theme.accent : Theme.line

        Icon {
            anchors.centerIn: parent
            visible: table.game.stockCount === 0 && table.game.wasteCount > 0
            path: Icons.redo
            color: Theme.muted
        }

        MouseArea {
            anchors.fill: parent
            enabled: table.game.active && !table.game.complete
            Accessible.role: Accessible.Button
            Accessible.name: table.game.stockCount > 0 ? qsTr("Deal from the stock") : qsTr("Turn the waste over")
            onClicked: table.game.draw()
        }
    }

    Repeater {
        model: 4
        Rectangle {
            required property int index
            readonly property bool target: table.game.hint.pile === SolitaireGame.Foundation && table.game.hint.slot === index
            x: table.slotX(3 + index)
            width: table.cardW
            height: table.cardH
            radius: Math.max(4, width * 0.12)
            color: Theme.subtle
            opacity: target ? 1 : 0.7
            border.width: target ? 2 : 0
            border.color: Theme.accent

            SuitIcon {
                anchors.centerIn: parent
                suit: parent.index
                size: table.cardW * 0.45
                color: Theme.muted
                opacity: 0.35
            }
        }
    }

    Repeater {
        model: 7
        Rectangle {
            required property int index
            readonly property bool target: table.game.hint.pile === SolitaireGame.Tableau && table.game.hint.slot === index
            x: table.slotX(index)
            y: table.tableauTop
            width: table.cardW
            height: table.cardH
            radius: Math.max(4, width * 0.12)
            color: "transparent"
            border.width: target ? 2 : 1
            border.color: target ? Theme.accent : Theme.line
        }
    }

    Repeater {
        model: 52

        PlayingCard {
            id: card

            required property int index
            readonly property var info: table.game.cards[index] || ({})
            readonly property var spot: table.placement[index] || ({ x: 0, y: 0 })
            readonly property bool held: table.dragGroup.indexOf(index) >= 0
            readonly property bool moving: xAnimation.running || yAnimation.running

            visible: info.pile !== undefined
            width: table.cardW
            height: table.cardH
            x: spot.x + (held ? table.dragOffset.x : 0)
            y: spot.y + (held ? table.dragOffset.y : 0)
            z: (held ? 2000 : moving ? 1000 : 0) + (info.pos || 0) + 1

            rank: info.rank || 1
            suit: info.suit || 0
            red: info.red === true
            faceUp: info.faceUp === true
            hinted: table.hintCards.indexOf(index) >= 0

            Behavior on x {
                enabled: !(table.dragging && card.held)
                NumberAnimation { id: xAnimation; duration: Theme.gentle; easing.type: Easing.OutCubic }
            }
            Behavior on y {
                enabled: !(table.dragging && card.held)
                NumberAnimation { id: yAnimation; duration: Theme.gentle; easing.type: Easing.OutCubic }
            }

            MouseArea {
                id: pointer

                property point pressedAt
                property bool moved: false

                anchors.fill: parent
                enabled: card.info.movable === true && table.game.active && !table.game.complete && !table.game.autoFinishing
                preventStealing: moved

                onPressed: (mouse) => {
                    pressedAt = mapToItem(table, mouse.x, mouse.y)
                    moved = false
                }
                onPositionChanged: (mouse) => {
                    const p = mapToItem(table, mouse.x, mouse.y)
                    if (!moved && Math.abs(p.x - pressedAt.x) + Math.abs(p.y - pressedAt.y) > 8) {
                        moved = true
                        table.beginDrag(card.index)
                    }
                    if (moved)
                        table.dragOffset = Qt.point(p.x - pressedAt.x, p.y - pressedAt.y)
                }
                onReleased: {
                    if (moved)
                        table.endDrag(card.index, true)
                    else if (table.game.tapToMove)
                        table.game.tapCard(card.index)
                    moved = false
                }
                onCanceled: {
                    if (moved)
                        table.endDrag(card.index, false)
                    moved = false
                }
            }
        }
    }
}
