import QtQuick
import Calm
import Solitaire

// The table for any of the games: the piles go where the game's `piles`
// layout puts them, and cards glide between places. Double-tap a card to
// send it home or to the leftmost column it fits on (it shakes if it fits
// nowhere); in Pyramid, TriPeaks and Golf a single tap plays it. Cards can
// also be dragged where you want them.
Item {
    id: table

    required property SolitaireGame game

    readonly property real columns: Math.max(1, game.tableColumns)
    readonly property real rows: Math.max(1, game.tableRows)
    readonly property real gap: Math.max(3, Math.round(width * 0.014))
    readonly property real cardW: Math.max(8, Math.floor(Math.min((width - (columns - 1) * gap) / columns,
                                                                  height / rows / 1.42)))
    readonly property real cardH: Math.round(cardW * 1.42)
    // The layout is centred when the table is wider than it needs.
    readonly property real offsetX: Math.max(0, Math.floor((width - (columns * (cardW + gap) - gap)) / 2))
    readonly property var hintCards: game.hint.cards || []

    // Cards being dragged and how far the pointer has moved them.
    property var dragGroup: []
    property bool dragging: false
    property point dragOffset: Qt.point(0, 0)

    // Cards shaking after a tap that found nowhere to go.
    property var shakeGroup: []
    property real shakeOffset: 0

    // Each pile's place, by "pile:slot".
    readonly property var places: {
        const out = {}
        const piles = game.piles
        for (let i = 0; i < piles.length; ++i)
            out[piles[i].pile + ":" + piles[i].slot] = piles[i]
        return out
    }
    readonly property var placement: layout(game.cards, places, width, height)

    function px(x) { return offsetX + x * (cardW + gap) }
    function py(y) { return Math.round(y * cardH) }

    // Where every card sits ("spots", by card index), and where each pile's
    // cards end ("ends", by "pile:slot").
    function layout(cards, places, w, h) {
        const down = {}
        const total = {}
        for (let i = 0; i < cards.length; ++i) {
            const c = cards[i]
            if (c.pile === undefined)
                continue
            const key = c.pile + ":" + c.slot
            total[key] = (total[key] || 0) + 1
            if (!c.faceUp)
                down[key] = (down[key] || 0) + 1
        }
        // Fanned piles squeeze together when they would run off the table.
        const steps = {}
        for (const key in places) {
            const p = places[key]
            if (p.fan !== 1)
                continue
            const n = total[key] || 0
            const d = down[key] || 0
            let ds = cardH * 0.12
            let us = cardH * 0.3
            const limit = p.bottom > 0 ? py(p.bottom) : h
            const room = Math.max(0, limit - py(p.y) - cardH)
            const need = d * ds + Math.max(0, n - d - 1) * us
            if (need > room && need > 0) {
                ds *= room / need
                us *= room / need
            }
            steps[key] = { down: d, ds: ds, us: us }
        }

        const fan = cardW * 0.32
        const out = []
        const ends = {}
        for (let i = 0; i < cards.length; ++i) {
            const c = cards[i]
            const key = c.pile + ":" + c.slot
            const p = places[key]
            if (c.pile === undefined || !p) {
                out.push({ x: 0, y: 0 })
                continue
            }
            let x = px(p.x)
            let y = py(p.y)
            if (p.fan === 1) {
                const s = steps[key]
                y += Math.min(c.pos, s.down) * s.ds + Math.max(0, c.pos - s.down) * s.us
            } else if (p.fan === 2) {
                x += Math.max(0, c.pos - (c.count - 3)) * fan
            }
            out.push({ x: x, y: y })
            ends[key] = Math.max(ends[key] || 0, y + cardH)
        }
        return { spots: out, ends: ends }
    }

    // The pile under a point: [pile, slot] or null. Columns reach down to
    // the bottom of the table; where places overlap (Pyramid, TriPeaks) the
    // one in front wins.
    function pileAt(x, y) {
        const piles = game.piles
        let best = null
        let bestTop = -1
        for (let i = 0; i < piles.length; ++i) {
            const p = piles[i]
            if (p.pile === SolitaireGame.Stock)
                continue
            const key = p.pile + ":" + p.slot
            const left = px(p.x)
            const top = py(p.y)
            let bottom = top + cardH
            if (p.fan === 1)
                bottom = p.bottom > 0 ? py(p.bottom) : height
            if (!p.outline && !placement.ends[key] && p.pile !== SolitaireGame.Waste)
                continue // an emptied place takes nothing
            if (x < left - gap / 2 || x > left + cardW + gap / 2 || y < top || y > bottom)
                continue
            if (top > bestTop) {
                best = [p.pile, p.slot]
                bestTop = top
            }
        }
        return best
    }

    // The card and every card lying on it.
    function groupOf(index) {
        const cards = game.cards
        const me = cards[index]
        const group = []
        for (let i = 0; i < cards.length; ++i) {
            const c = cards[i]
            if (c.pile === me.pile && c.slot === me.slot && c.pos >= me.pos)
                group.push(i)
        }
        return group
    }

    function beginDrag(index) {
        dragOffset = Qt.point(0, 0)
        dragGroup = groupOf(index)
        dragging = true
    }

    function shakeCard(index) {
        shakeGroup = groupOf(index)
        shake.restart()
    }

    function quickMove(index) {
        if (!game.quickMove(index))
            shakeCard(index)
    }

    function tapCard(index) {
        if (!game.tapCard(index))
            shakeCard(index)
    }

    SequentialAnimation {
        id: shake
        // A small shake reads as "no" even with reduced motion.
        NumberAnimation { target: table; property: "shakeOffset"; to: -7; duration: 45 }
        NumberAnimation { target: table; property: "shakeOffset"; to: 7; duration: 70 }
        NumberAnimation { target: table; property: "shakeOffset"; to: -5; duration: 60 }
        NumberAnimation { target: table; property: "shakeOffset"; to: 4; duration: 55 }
        NumberAnimation { target: table; property: "shakeOffset"; to: 0; duration: 45 }
        ScriptAction { script: table.shakeGroup = [] }
    }

    function endDrag(index, drop) {
        const spot = placement.spots[index]
        const target = drop ? pileAt(spot.x + dragOffset.x + cardW / 2, spot.y + dragOffset.y + cardH / 3) : null
        // Turn animations back on before the cards leave the pointer, so
        // they glide to wherever they end up.
        dragging = false
        dragGroup = []
        dragOffset = Qt.point(0, 0)
        if (target)
            game.dropCard(index, target[0], target[1])
    }

    // Empty places: stock, waste, foundations, cells and columns.
    Repeater {
        model: table.game.piles

        Rectangle {
            id: slot

            required property var modelData
            readonly property bool stock: modelData.pile === SolitaireGame.Stock
            readonly property bool target: table.game.hint.pile === modelData.pile && table.game.hint.slot === modelData.slot
                                           && (table.game.hint.cards || []).length > 0
            readonly property bool home: modelData.pile === SolitaireGame.Foundation || modelData.pile === SolitaireGame.Cell

            x: table.px(modelData.x)
            y: table.py(modelData.y)
            width: table.cardW
            height: table.cardH
            radius: Math.max(4, width * 0.12)
            visible: modelData.outline
            color: home ? Theme.subtle : "transparent"
            opacity: home && !target ? 0.7 : 1
            border.width: target || (stock && table.game.hint.stock === true) ? 2 : home ? 0 : 1
            border.color: target || (stock && table.game.hint.stock === true) ? Theme.accent : Theme.line

            SuitIcon {
                anchors.centerIn: parent
                visible: slot.modelData.mark >= 0
                suit: Math.max(0, slot.modelData.mark)
                size: table.cardW * 0.45
                color: Theme.muted
                opacity: 0.35
            }

            Icon {
                anchors.centerIn: parent
                visible: slot.stock && table.game.stockCount === 0 && table.game.canDraw
                path: Icons.redo
                color: Theme.muted
            }

            MouseArea {
                anchors.fill: parent
                enabled: slot.stock && table.game.active && !table.game.complete
                Accessible.role: Accessible.Button
                Accessible.name: table.game.stockCount > 0 ? qsTr("Deal from the stock") : qsTr("Turn the waste over")
                onClicked: table.game.draw()
            }
        }
    }

    Repeater {
        model: table.game.cards.length

        PlayingCard {
            id: card

            required property int index
            readonly property var info: table.game.cards[index] || ({})
            readonly property var spot: table.placement.spots[index] || ({ x: 0, y: 0 })
            readonly property bool held: table.dragGroup.indexOf(index) >= 0
            readonly property bool moving: xAnimation.running || yAnimation.running

            visible: info.pile !== undefined
            width: table.cardW
            height: table.cardH
            x: spot.x + (held ? table.dragOffset.x : 0)
            y: spot.y + (held ? table.dragOffset.y : 0)
            // Later cards in a pile, and places further down, lie on top.
            z: (held ? 2000 : moving ? 1000 : 0) + (info.pos || 0) + 1 + spot.y / 10000

            rank: info.rank || 1
            suit: info.suit || 0
            red: info.red === true
            faceUp: info.faceUp === true
            hinted: table.hintCards.indexOf(index) >= 0 || table.game.selectedCard === index
            nudge: table.shakeGroup.indexOf(index) >= 0 ? table.shakeOffset : 0

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
                }
                onClicked: {
                    if (!moved && table.game.tapToPlay)
                        table.tapCard(card.index)
                }
                onDoubleClicked: {
                    if (table.game.tapToPlay)
                        table.tapCard(card.index)
                    else
                        table.quickMove(card.index)
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
