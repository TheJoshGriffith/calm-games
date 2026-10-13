import QtQuick
import Calm
import Sudoku

// The 9x9 grid. Tap a cell to select it, drag to select every cell passed
// over; with Multi-select on, taps add or remove cells instead.
Rectangle {
    id: board

    required property SudokuGame game
    readonly property real cellSize: (width - 4) / 9
    // Read once per change; delegates index into it, so they persist and animate.
    readonly property var cells: game.cells
    readonly property int primary: game.selected
    readonly property int selectedDigit: primary >= 0 && cells.length === 81 ? cells[primary].digit : 0
    readonly property bool multiple: game.selection.length > 1
    // selectedFlags[i] is true when cell i is in the selection.
    readonly property var selectedFlags: {
        const flags = new Array(81).fill(false)
        for (const c of game.selection)
            flags[c] = true
        return flags
    }

    // Corner marks fill these slots in order: corners first, then edge
    // middles, then the centre (column, row on a 3x3 grid).
    readonly property var cornerSlots: [[0, 0], [2, 0], [0, 2], [2, 2], [1, 0], [1, 2], [0, 1], [2, 1], [1, 1]]

    function isPeer(a, b) {
        if (a < 0 || b < 0 || a === b)
            return false
        const ra = Math.floor(a / 9), ca = a % 9, rb = Math.floor(b / 9), cb = b % 9
        return ra === rb || ca === cb
            || (Math.floor(ra / 3) === Math.floor(rb / 3) && Math.floor(ca / 3) === Math.floor(cb / 3))
    }

    function digitsOf(mask) {
        const out = []
        for (let d = 1; d <= 9; ++d)
            if (mask & (1 << d))
                out.push(d)
        return out
    }

    function cellAt(x, y) {
        if (x < 2 || y < 2 || x >= width - 2 || y >= height - 2)
            return -1
        const col = Math.min(8, Math.floor((x - 2) / cellSize))
        const row = Math.min(8, Math.floor((y - 2) / cellSize))
        return row * 9 + col
    }

    implicitWidth: 358
    implicitHeight: width
    radius: 14
    color: Theme.surface
    border.color: Theme.strongLine
    border.width: 2
    clip: true

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

                readonly property bool selected: board.selectedFlags[index]
                readonly property bool matching: board.game.highlightMatching && board.selectedDigit > 0
                                                 && modelData.digit === board.selectedDigit
                readonly property bool peer: board.game.highlightPeers && board.isPeer(index, board.primary)
                readonly property bool coloured: modelData.colour > 0
                readonly property var cornerDigits: modelData.digit === 0 ? board.digitsOf(modelData.corner) : []
                readonly property var centreDigits: modelData.digit === 0 ? board.digitsOf(modelData.centre) : []

                width: board.cellSize
                height: board.cellSize

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Row %1, column %2, %3")
                                 .arg(Math.floor(index / 9) + 1).arg(index % 9 + 1)
                                 .arg(modelData.digit > 0 ? modelData.digit : qsTr("empty"))
                Accessible.onPressAction: board.game.selectOnly(cell.index)

                // Player colour: the bottom layer.
                Rectangle {
                    anchors.fill: parent
                    color: CellColours.colour(cell.modelData.colour)
                    Behavior on color { ColorAnimation { duration: Theme.quick } }
                }

                // Selection, matching and peer highlights, laid over the colour
                // and let through it partly so both stay visible.
                Rectangle {
                    anchors.fill: parent
                    color: cell.selected ? Theme.selection : cell.matching ? Theme.match
                         : cell.peer ? Theme.peer : "transparent"
                    opacity: cell.coloured ? (cell.selected ? 0.6 : 0.45) : 1
                    Behavior on color { ColorAnimation { duration: Theme.quick } }
                }

                // A ring marks selected cells whenever a fill alone could be
                // ambiguous: several cells selected, or a coloured cell.
                Rectangle {
                    anchors.fill: parent
                    anchors.margins: 1.5
                    radius: 3
                    color: "transparent"
                    border.color: Theme.accent
                    border.width: 2
                    visible: cell.selected && (board.multiple || cell.coloured)
                }

                CalmText {
                    id: digitText
                    anchors.centerIn: parent
                    visible: cell.modelData.digit > 0
                    text: cell.modelData.digit
                    font.pixelSize: 22
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
                        readonly property var slot: board.cornerSlots[index]

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

    // Grid lines drawn over the cells: hairlines, with stronger box borders.
    Repeater {
        model: 8
        Rectangle {
            required property int index
            x: 2 + (index + 1) * board.cellSize - width / 2
            y: 2
            width: (index + 1) % 3 === 0 ? 2 : 1
            height: board.height - 4
            color: (index + 1) % 3 === 0 ? Theme.strongLine : Theme.line
        }
    }
    Repeater {
        model: 8
        Rectangle {
            required property int index
            x: 2
            y: 2 + (index + 1) * board.cellSize - height / 2
            width: board.width - 4
            height: (index + 1) % 3 === 0 ? 2 : 1
            color: (index + 1) % 3 === 0 ? Theme.strongLine : Theme.line
        }
    }

    // Selection input for the whole grid, so a drag can sweep across cells.
    MouseArea {
        anchors.fill: parent
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
