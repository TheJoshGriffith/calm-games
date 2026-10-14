import QtQuick
import Calm
import Sudoku

// The 9x9 grid. Tap a cell to select it, drag to select every cell passed
// over; with Multi-select on, taps add or remove cells instead.
//
// Variants draw their clues in layers: shading and lines (thermometers,
// arrows, whispers...) between the cell highlights and the digits; region
// borders, cages, dots and letters over the grid lines; sandwich and little
// killer sums in a margin around the grid.
Item {
    id: board

    required property SudokuGame game
    readonly property var clues: game.clues
    // Room around the grid for clues outside it.
    readonly property real margin: clues.outside === true ? Math.round(width * 0.085) : 0
    readonly property real gridSize: width - 2 * margin
    readonly property real cellSize: (gridSize - 4) / 9
    // Read once per change; delegates index into it, so they persist and animate.
    readonly property var cells: game.cells
    readonly property int primary: game.selected
    readonly property int selectedDigit: primary >= 0 && cells.length === 81 ? cells[primary].digit : 0
    readonly property bool multiple: game.selection.length > 1
    // A digit pressed with nothing selected: its cells light up and every cell
    // it can no longer go in is shaded, leaving its possible places clear.
    readonly property int focusDigit: game.selection.length === 0 ? game.focusDigit : 0
    // focusBlocked[i] is true when focusDigit can't go in cell i.
    readonly property var focusBlocked: {
        board.cells // re-read whenever the board changes
        return focusDigit === 0 ? new Array(81).fill(false) : game.blockedFor(focusDigit)
    }
    // selectedFlags[i] is true when cell i is in the selection.
    readonly property var selectedFlags: {
        const flags = new Array(81).fill(false)
        for (const c of game.selection)
            flags[c] = true
        return flags
    }
    readonly property bool jigsaw: clues.jigsaw === true

    // Corner marks fill these slots in order: corners first, then edge
    // middles, then the centre (column, row on a 3x3 grid).
    readonly property var cornerSlots: [[0, 0], [2, 0], [0, 2], [2, 2], [1, 0], [1, 2], [0, 1], [2, 1], [1, 1]]
    // A cell holding a cage sum keeps its top-left corner for the sum.
    readonly property var labelledSlots: [[2, 0], [0, 2], [2, 2], [1, 0], [1, 2], [0, 1], [2, 1], [1, 1], [0, 0]]
    readonly property var cageAnchors: {
        const flags = new Array(81).fill(false)
        for (const cage of (clues.cages || []))
            flags[Math.min.apply(null, cage.cells)] = true
        return flags
    }

    function digitsOf(mask) {
        const out = []
        for (let d = 1; d <= 9; ++d)
            if (mask & (1 << d))
                out.push(d)
        return out
    }

    // Cell under a point in the grid's coordinates.
    function cellAt(x, y) {
        if (x < 2 || y < 2 || x >= gridSize - 2 || y >= gridSize - 2)
            return -1
        const col = Math.min(8, Math.floor((x - 2) / cellSize))
        const row = Math.min(8, Math.floor((y - 2) / cellSize))
        return row * 9 + col
    }

    // Centre of a cell (or of a row/column just outside), in grid coordinates.
    function cx(col) { return 2 + (col + 0.5) * cellSize }
    function cy(row) { return 2 + (row + 0.5) * cellSize }
    function withAlpha(c, a) { return Qt.rgba(c.r, c.g, c.b, a) }

    implicitWidth: 358
    implicitHeight: width

    // Sandwich sums: left of each row and above each column.
    Repeater {
        model: board.clues.rowSandwich ? 18 : 0
        CalmText {
            required property int index
            readonly property bool isRow: index < 9
            readonly property int value: isRow ? board.clues.rowSandwich[index] : board.clues.colSandwich[index - 9]
            visible: value >= 0
            width: board.margin
            height: board.margin
            x: isRow ? 0 : board.margin + board.cx(index - 9) - width / 2
            y: isRow ? board.margin + board.cy(index) - height / 2 : 0
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
            text: value
            font.pixelSize: Math.min(18, Math.max(10, Math.round(board.cellSize * 0.4)))
            font.weight: Font.Bold
            color: Theme.ink
            Accessible.role: Accessible.StaticText
            Accessible.name: isRow ? qsTr("Row %1 sandwich %2").arg(index + 1).arg(value)
                                   : qsTr("Column %1 sandwich %2").arg(index - 8).arg(value)
        }
    }

    // Little killer sums, each with a small arrow along its diagonal.
    Repeater {
        model: board.clues.littleKillers || []
        Item {
            required property var modelData
            // The clue sits in the margin, a little way along its diagonal
            // out of the grid, pulled in where it would leave the board.
            readonly property real px: board.margin + board.cx(modelData.col) + (modelData.col < 0 ? 0.2 : modelData.col > 8 ? -0.2 : 0) * board.cellSize
            readonly property real py: board.margin + board.cy(modelData.row) + (modelData.row < 0 ? 0.2 : modelData.row > 8 ? -0.2 : 0) * board.cellSize
            x: px - width / 2
            y: py - height / 2
            width: board.margin
            height: board.margin
            Accessible.role: Accessible.StaticText
            Accessible.name: qsTr("Diagonal sum %1").arg(modelData.sum)

            CalmText {
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: -parent.modelData.dc * board.margin * 0.16
                anchors.verticalCenterOffset: -parent.modelData.dr * board.margin * 0.16
                text: parent.modelData.sum
                font.pixelSize: Math.min(16, Math.max(9, Math.round(board.cellSize * 0.42)))
                font.weight: Font.Bold
                Accessible.ignored: true
            }
            Canvas {
                anchors.fill: parent
                readonly property color ink: Theme.strongLine
                onInkChanged: requestPaint()
                onWidthChanged: requestPaint()
                onPaint: {
                    const ctx = getContext("2d")
                    ctx.reset()
                    const d = parent.modelData
                    const w = width, h = height
                    // A short arrow in the corner the diagonal heads for.
                    const tipX = w / 2 + d.dc * w * 0.44, tipY = h / 2 + d.dr * h * 0.44
                    const tailX = w / 2 + d.dc * w * 0.14, tailY = h / 2 + d.dr * h * 0.14
                    ctx.strokeStyle = ink
                    ctx.lineWidth = 1.6
                    ctx.lineCap = "round"
                    ctx.beginPath()
                    ctx.moveTo(tailX, tailY)
                    ctx.lineTo(tipX, tipY)
                    const head = w * 0.18
                    ctx.moveTo(tipX, tipY)
                    ctx.lineTo(tipX - d.dc * head, tipY)
                    ctx.moveTo(tipX, tipY)
                    ctx.lineTo(tipX, tipY - d.dr * head)
                    ctx.stroke()
                }
            }
        }
    }

    Rectangle {
        id: frame
        x: board.margin
        y: board.margin
        width: board.gridSize
        height: board.gridSize
        radius: 14
        color: Theme.surface
        border.color: Theme.strongLine
        border.width: 2
        clip: true

        // Cell backgrounds: player colour, then selection and peer highlights.
        Grid {
            x: 2
            y: 2
            columns: 9

            Repeater {
                model: 81

                Item {
                    id: back

                    required property int index
                    readonly property var modelData: board.cells.length === 81 ? board.cells[index] : { digit: 0, colour: 0 }
                    readonly property bool selected: board.selectedFlags[index]
                    readonly property bool matching: board.game.highlightMatching && board.selectedDigit > 0
                                                     && modelData.digit === board.selectedDigit
                    readonly property bool peer: board.game.highlightPeers && board.game.sameHouse(index, board.primary)
                    readonly property bool focused: board.focusDigit > 0 && modelData.digit === board.focusDigit
                    readonly property bool blocked: board.focusBlocked[index] === true
                    readonly property bool coloured: modelData.colour > 0

                    width: board.cellSize
                    height: board.cellSize

                    // Player colour: the bottom layer.
                    Rectangle {
                        anchors.fill: parent
                        color: CellColours.colour(back.modelData.colour)
                        Behavior on color { ColorAnimation { duration: Theme.quick } }
                    }

                    // Windoku's four extra boxes are softly shaded.
                    Rectangle {
                        anchors.fill: parent
                        visible: board.clues.windoku === true && !back.coloured
                                 && [1, 2, 3, 5, 6, 7].indexOf(Math.floor(back.index / 9)) >= 0
                                 && [1, 2, 3, 5, 6, 7].indexOf(back.index % 9) >= 0
                        color: Theme.subtle
                    }

                    // Selection, matching and peer highlights, laid over the
                    // colour and let through it partly so both stay visible.
                    Rectangle {
                        anchors.fill: parent
                        // A focused digit looks like a selected cell's matches and peers.
                        color: back.selected ? Theme.selection : back.matching || back.focused ? Theme.match
                             : back.blocked || back.peer ? Theme.peer : "transparent"
                        opacity: back.coloured ? (back.selected ? 0.6 : 0.45) : 1
                        Behavior on color { ColorAnimation { duration: Theme.quick } }
                    }
                }
            }
        }

        // Grid lines over the cell highlights and under the clues:
        // hairlines, with stronger box borders (jigsaw regions draw their own
        // borders above the clues).
        Repeater {
            model: 8
            Rectangle {
                required property int index
                readonly property bool box: !board.jigsaw && (index + 1) % 3 === 0
                x: 2 + (index + 1) * board.cellSize - width / 2
                y: 2
                width: box ? 2 : 1
                height: frame.height - 4
                color: box ? Theme.strongLine : Theme.line
            }
        }
        Repeater {
            model: 8
            Rectangle {
                required property int index
                readonly property bool box: !board.jigsaw && (index + 1) % 3 === 0
                x: 2
                y: 2 + (index + 1) * board.cellSize - height / 2
                width: frame.width - 4
                height: box ? 2 : 1
                color: box ? Theme.strongLine : Theme.line
            }
        }

        // Clue shading and lines, under the digits.
        Canvas {
            id: underlay
            anchors.fill: parent
            readonly property var clues: board.clues
            readonly property bool dark: Theme.dark
            onCluesChanged: requestPaint()
            onDarkChanged: requestPaint()
            onWidthChanged: requestPaint()

            function centre(cell) { return [board.cx(cell % 9), board.cy(Math.floor(cell / 9))] }

            function polyline(ctx, cells, colour, width) {
                if (!cells || cells.length < 2)
                    return
                ctx.strokeStyle = colour
                ctx.lineWidth = width
                ctx.lineCap = "round"
                ctx.lineJoin = "round"
                ctx.beginPath()
                const first = centre(cells[0])
                ctx.moveTo(first[0], first[1])
                for (let i = 1; i < cells.length; ++i) {
                    const p = centre(cells[i])
                    ctx.lineTo(p[0], p[1])
                }
                ctx.stroke()
            }

            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                const c = clues
                const s = board.cellSize
                if (!c || !c.regions)
                    return

                if (c.diagonals) {
                    ctx.strokeStyle = board.withAlpha(Theme.strongLine, 0.45)
                    ctx.lineWidth = 1.5
                    ctx.beginPath()
                    ctx.moveTo(2, 2)
                    ctx.lineTo(2 + 9 * s, 2 + 9 * s)
                    ctx.moveTo(2 + 9 * s, 2)
                    ctx.lineTo(2, 2 + 9 * s)
                    ctx.stroke()
                }
                // Even: a soft square. Odd: a soft circle.
                ctx.fillStyle = board.withAlpha(Theme.strongLine, Theme.dark ? 0.3 : 0.2)
                for (let i = 0; i < 81; ++i) {
                    const p = c.parity[i]
                    if (!p)
                        continue
                    const x = 2 + (i % 9) * s, y = 2 + Math.floor(i / 9) * s
                    ctx.beginPath()
                    if (p === 1) {
                        const inset = s * 0.12, r = s * 0.12
                        const w = s - 2 * inset
                        ctx.roundedRect(x + inset, y + inset, w, w, r, r)
                    } else {
                        ctx.arc(x + s / 2, y + s / 2, s * 0.4, 0, 2 * Math.PI)
                    }
                    ctx.fill()
                }

                for (const line of c.whispers)
                    polyline(ctx, line, CellColours.whisper, s * 0.2)
                for (const line of c.renbans)
                    polyline(ctx, line, CellColours.renban, s * 0.26)
                for (const line of c.palindromes)
                    polyline(ctx, line, CellColours.palindrome, s * 0.2)

                // Thermometers: a bulb and a thick line.
                for (const line of c.thermos) {
                    polyline(ctx, line, CellColours.thermo, s * 0.3)
                    const b = centre(line[0])
                    ctx.fillStyle = CellColours.thermo
                    ctx.beginPath()
                    ctx.arc(b[0], b[1], s * 0.37, 0, 2 * Math.PI)
                    ctx.fill()
                }

                // Arrows: a ring, then a thin shaft from its edge with a head.
                const ink = board.withAlpha(Theme.strongLine, 0.7)
                for (const arrow of c.arrows) {
                    const o = centre(arrow.circle)
                    const ring = s * 0.38
                    ctx.strokeStyle = ink
                    ctx.lineWidth = 1.6
                    ctx.beginPath()
                    ctx.arc(o[0], o[1], ring, 0, 2 * Math.PI)
                    ctx.stroke()

                    const pts = [o].concat(arrow.path.map(cell => centre(cell)))
                    const dx = pts[1][0] - o[0], dy = pts[1][1] - o[1]
                    const len = Math.sqrt(dx * dx + dy * dy)
                    ctx.lineCap = "round"
                    ctx.lineJoin = "round"
                    ctx.beginPath()
                    ctx.moveTo(o[0] + dx / len * ring, o[1] + dy / len * ring)
                    for (let i = 1; i < pts.length; ++i)
                        ctx.lineTo(pts[i][0], pts[i][1])
                    ctx.stroke()
                    // Head on the last cell, pointing along the last step.
                    const a = pts[pts.length - 2], z = pts[pts.length - 1]
                    const angle = Math.atan2(z[1] - a[1], z[0] - a[0])
                    const head = s * 0.2
                    ctx.beginPath()
                    ctx.moveTo(z[0] - head * Math.cos(angle - 0.5), z[1] - head * Math.sin(angle - 0.5))
                    ctx.lineTo(z[0], z[1])
                    ctx.lineTo(z[0] - head * Math.cos(angle + 0.5), z[1] - head * Math.sin(angle + 0.5))
                    ctx.stroke()
                }
            }
        }

        // Digits and marks.
        Grid {
            x: 2
            y: 2
            columns: 9

            Repeater {
                model: 81

                Item {
                    id: cell

                    required property int index
                    readonly property var modelData: board.cells.length === 81
                        ? board.cells[index]
                        : { digit: 0, given: false, corner: 0, centre: 0, colour: 0, mistake: false }
                    readonly property var cornerDigits: modelData.digit === 0 ? board.digitsOf(modelData.corner) : []
                    readonly property var centreDigits: modelData.digit === 0 ? board.digitsOf(modelData.centre) : []

                    width: board.cellSize
                    height: board.cellSize

                    Accessible.role: Accessible.Button
                    Accessible.name: qsTr("Row %1, column %2, %3")
                                     .arg(Math.floor(index / 9) + 1).arg(index % 9 + 1)
                                     .arg(modelData.digit > 0 ? modelData.digit : qsTr("empty"))
                    Accessible.onPressAction: board.game.selectOnly(cell.index)

                    // A ring marks selected cells whenever a fill alone could be
                    // ambiguous: several cells selected, or a coloured cell.
                    Rectangle {
                        anchors.fill: parent
                        anchors.margins: 1.5
                        radius: 3
                        color: "transparent"
                        border.color: Theme.accent
                        border.width: 2
                        visible: board.selectedFlags[cell.index] === true && (board.multiple || cell.modelData.colour > 0)
                    }

                    CalmText {
                        id: digitText
                        anchors.centerIn: parent
                        visible: cell.modelData.digit > 0
                        text: cell.modelData.digit
                        font.pixelSize: Math.min(22, Math.round(board.cellSize * 0.58))
                        font.weight: cell.modelData.given ? Font.Bold : Font.DemiBold
                        color: !cell.modelData.mistake ? (cell.modelData.given ? Theme.ink : Theme.entry)
                             : cell.modelData.given ? Qt.tint(Theme.ink, Qt.rgba(Theme.danger.r, Theme.danger.g, Theme.danger.b, 0.3))
                             : Theme.danger
                        onTextChanged: if (visible && Theme.quick > 0) pop.restart()

                        NumberAnimation on scale {
                            id: pop
                            running: false
                            from: 0.75
                            to: 1
                            duration: Theme.gentle
                            easing.type: Easing.OutBack
                        }
                    }

                    // Corner marks: small digits around the edges.
                    Repeater {
                        model: cell.cornerDigits

                        CalmText {
                            required property int index
                            required property int modelData
                            readonly property var slot: (board.cageAnchors[cell.index] ? board.labelledSlots : board.cornerSlots)[index]

                            x: 2 + slot[0] * (cell.width - 4) / 3
                            y: 1 + slot[1] * (cell.height - 2) / 3
                            width: (cell.width - 4) / 3
                            height: (cell.height - 2) / 3
                            horizontalAlignment: slot[0] === 0 ? Text.AlignLeft : slot[0] === 2 ? Text.AlignRight : Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            text: modelData
                            font.pixelSize: 11
                            font.weight: Font.Bold
                            color: Theme.muted
                        }
                    }

                    // Centre marks: a small row across the middle, shrinking and
                    // wrapping to two lines when there are many.
                    CalmText {
                        anchors.centerIn: parent
                        width: cell.width - 8
                        height: cell.height - 12
                        visible: cell.centreDigits.length > 0
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                        lineHeight: 0.85
                        text: {
                            const d = cell.centreDigits
                            if (d.length <= 5)
                                return d.join("")
                            const half = Math.ceil(d.length / 2)
                            return d.slice(0, half).join("") + "\n" + d.slice(half).join("")
                        }
                        font.pixelSize: cell.centreDigits.length <= 3 ? 14 : 11
                        fontSizeMode: Text.Fit
                        minimumPixelSize: 7
                        font.weight: Font.Bold
                        color: Theme.muted
                    }
                }
            }
        }

        // Region borders for jigsaw, and killer cages, over the grid lines.
        Canvas {
            id: overlay
            anchors.fill: parent
            readonly property var clues: board.clues
            readonly property bool dark: Theme.dark
            onCluesChanged: requestPaint()
            onDarkChanged: requestPaint()
            onWidthChanged: requestPaint()

            // A dashed segment, drawn by hand so it looks the same everywhere.
            function dashed(ctx, x0, y0, x1, y1) {
                const len = Math.sqrt((x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0))
                if (len <= 0)
                    return
                const dash = 3, gap = 2.5
                const ux = (x1 - x0) / len, uy = (y1 - y0) / len
                for (let t = 0; t < len; t += dash + gap) {
                    const e = Math.min(len, t + dash)
                    ctx.moveTo(x0 + ux * t, y0 + uy * t)
                    ctx.lineTo(x0 + ux * e, y0 + uy * e)
                }
            }

            onPaint: {
                const ctx = getContext("2d")
                ctx.reset()
                const c = clues
                const s = board.cellSize
                if (!c || !c.regions)
                    return

                if (c.jigsaw) {
                    ctx.strokeStyle = Theme.strongLine
                    ctx.lineWidth = 2
                    ctx.lineCap = "square"
                    ctx.beginPath()
                    for (let i = 0; i < 81; ++i) {
                        const r = Math.floor(i / 9), col = i % 9
                        if (col < 8 && c.regions[i] !== c.regions[i + 1]) {
                            ctx.moveTo(2 + (col + 1) * s, 2 + r * s)
                            ctx.lineTo(2 + (col + 1) * s, 2 + (r + 1) * s)
                        }
                        if (r < 8 && c.regions[i] !== c.regions[i + 9]) {
                            ctx.moveTo(2 + col * s, 2 + (r + 1) * s)
                            ctx.lineTo(2 + (col + 1) * s, 2 + (r + 1) * s)
                        }
                    }
                    ctx.stroke()
                }

                if (c.cages.length > 0) {
                    const inset = Math.max(3, s * 0.1)
                    // Room left at each cage's first cell for its sum.
                    const labelW = s * 0.44, labelH = s * 0.36
                    ctx.strokeStyle = board.withAlpha(Theme.ink, Theme.dark ? 0.7 : 0.6)
                    ctx.lineWidth = 1
                    ctx.beginPath()
                    for (const cage of c.cages) {
                        const inCage = new Array(81).fill(false)
                        for (const cell of cage.cells)
                            inCage[cell] = true
                        const has = (r, col) => r >= 0 && r < 9 && col >= 0 && col < 9 && inCage[r * 9 + col]
                        const anchor = Math.min.apply(null, cage.cells)
                        for (const cell of cage.cells) {
                            const r = Math.floor(cell / 9), col = cell % 9
                            const left = 2 + col * s, top = 2 + r * s, right = left + s, bottom = top + s
                            const labelled = cell === anchor
                            // Each open side, inset; corners meet the next
                            // side's line, reaching into the neighbour at a
                            // concave turn.
                            if (!has(r - 1, col)) {
                                const x0 = has(r, col - 1) ? (has(r - 1, col - 1) ? left - inset : left) : left + inset
                                const x1 = has(r, col + 1) ? (has(r - 1, col + 1) ? right + inset : right) : right - inset
                                dashed(ctx, labelled ? left + labelW : x0, top + inset, x1, top + inset)
                            }
                            if (!has(r + 1, col)) {
                                const x0 = has(r, col - 1) ? (has(r + 1, col - 1) ? left - inset : left) : left + inset
                                const x1 = has(r, col + 1) ? (has(r + 1, col + 1) ? right + inset : right) : right - inset
                                dashed(ctx, x0, bottom - inset, x1, bottom - inset)
                            }
                            if (!has(r, col - 1)) {
                                const y0 = has(r - 1, col) ? (has(r - 1, col - 1) ? top - inset : top) : top + inset
                                const y1 = has(r + 1, col) ? (has(r + 1, col - 1) ? bottom + inset : bottom) : bottom - inset
                                dashed(ctx, left + inset, labelled ? top + labelH : y0, left + inset, y1)
                            }
                            if (!has(r, col + 1)) {
                                const y0 = has(r - 1, col) ? (has(r - 1, col + 1) ? top - inset : top) : top + inset
                                const y1 = has(r + 1, col) ? (has(r + 1, col + 1) ? bottom + inset : bottom) : bottom - inset
                                dashed(ctx, right - inset, y0, right - inset, y1)
                            }
                        }
                    }
                    ctx.stroke()
                }
            }
        }

        // Cage sums, in each cage's top-left cell.
        Repeater {
            model: board.clues.cages || []
            CalmText {
                required property var modelData
                readonly property int anchor: Math.min.apply(null, modelData.cells)
                x: 2 + (anchor % 9) * board.cellSize + 2
                y: 2 + Math.floor(anchor / 9) * board.cellSize + 1
                text: modelData.sum
                font.pixelSize: Math.min(12, Math.max(8, Math.round(board.cellSize * 0.25)))
                font.weight: Font.Bold
                color: Theme.ink
                Accessible.role: Accessible.StaticText
                Accessible.name: qsTr("Cage of %1 cells, sum %2").arg(modelData.cells.length).arg(modelData.sum)
            }
        }

        // Kropki dots and XV letters on the edges between cells.
        Repeater {
            model: board.clues.dots || []
            Item {
                id: mark
                required property var modelData
                readonly property bool across: modelData.b === modelData.a + 1
                readonly property bool letter: modelData.kind === "x" || modelData.kind === "v"
                readonly property real midX: board.cx(modelData.a % 9) + (across ? board.cellSize / 2 : 0)
                readonly property real midY: board.cy(Math.floor(modelData.a / 9)) + (across ? 0 : board.cellSize / 2)
                width: letter ? board.cellSize * 0.34 : board.cellSize * 0.24
                height: width
                x: midX - width / 2
                y: midY - height / 2

                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    // White and black as the rules name them, in either theme;
                    // an XV letter sits on a plain patch.
                    color: mark.letter ? Theme.surface
                         : mark.modelData.kind === "black" ? (Theme.dark ? Theme.knob : Theme.ink)
                         : (Theme.dark ? Theme.ink : Theme.surface)
                    border.color: mark.letter ? "transparent" : Theme.ink
                    border.width: mark.letter ? 0 : 1.4
                }
                CalmText {
                    anchors.centerIn: parent
                    visible: mark.letter
                    text: mark.modelData.kind === "x" ? "X" : "V"
                    font.pixelSize: Math.min(17, Math.max(9, Math.round(board.cellSize * 0.42)))
                    font.weight: Font.ExtraBold
                    color: Theme.ink
                }
            }
        }
    }

    // Selection input for the whole grid, so a drag can sweep across cells.
    MouseArea {
        anchors.fill: frame
        preventStealing: true

        property int lastCell: -1
        // During a multi-select drag: true adds cells, false removes them,
        // following what the first touch did.
        property bool adding: true

        onPressed: (mouse) => {
            const c = board.cellAt(mouse.x, mouse.y)
            lastCell = c
            if (c < 0)
                return
            if (board.game.multiSelect) {
                adding = !board.game.isSelected(c)
                if (adding)
                    board.game.addToSelection(c)
                else
                    board.game.removeFromSelection(c)
            } else {
                adding = true
                board.game.selectOnly(c)
            }
        }
        onPositionChanged: (mouse) => {
            const c = board.cellAt(mouse.x, mouse.y)
            if (c < 0 || c === lastCell)
                return
            lastCell = c
            if (adding)
                board.game.addToSelection(c)
            else
                board.game.removeFromSelection(c)
        }
    }
}
