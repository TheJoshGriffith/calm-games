import QtQuick
import Calm
import Sudoku

// The 9x9 grid. Tap a cell to select it.
Rectangle {
    id: board

    required property SudokuGame game
    readonly property real cellSize: (width - 4) / 9
    // Read once per change; delegates index into it, so they persist and animate.
    readonly property var cells: game.cells
    readonly property int selectedDigit: game.selected >= 0 && cells.length === 81 ? cells[game.selected].digit : 0

    function isPeer(a, b) {
        if (a < 0 || b < 0 || a === b)
            return false
        const ra = Math.floor(a / 9), ca = a % 9, rb = Math.floor(b / 9), cb = b % 9
        return ra === rb || ca === cb
            || (Math.floor(ra / 3) === Math.floor(rb / 3) && Math.floor(ca / 3) === Math.floor(cb / 3))
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

            Rectangle {
                id: cell

                required property int index
                readonly property var modelData: board.cells.length === 81 ? board.cells[index]
                                                                          : { digit: 0, given: false, notes: 0, mistake: false }

                readonly property bool selected: index === board.game.selected
                readonly property bool matching: board.game.highlightMatching && board.selectedDigit > 0
                                                 && modelData.digit === board.selectedDigit
                readonly property bool peer: board.game.highlightPeers && board.isPeer(index, board.game.selected)

                width: board.cellSize
                height: board.cellSize
                color: selected ? Theme.selection : matching ? Theme.match : peer ? Theme.peer : "transparent"
                Behavior on color { ColorAnimation { duration: Theme.quick } }

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Row %1, column %2, %3")
                                 .arg(Math.floor(index / 9) + 1).arg(index % 9 + 1)
                                 .arg(modelData.digit > 0 ? modelData.digit : qsTr("empty"))

                CalmText {
                    id: digitText
                    anchors.centerIn: parent
                    visible: cell.modelData.digit > 0
                    text: cell.modelData.digit
                    font.pixelSize: 22
                    font.weight: cell.modelData.given ? Font.Bold : Font.DemiBold
                    color: cell.modelData.given ? Theme.ink
                         : (board.game.showMistakes && cell.modelData.mistake) ? Theme.danger
                         : Theme.entry
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

                Grid {
                    anchors.fill: parent
                    anchors.margins: 2
                    columns: 3
                    visible: cell.modelData.digit === 0 && cell.modelData.notes !== 0
                    Repeater {
                        model: 9
                        CalmText {
                            required property int index
                            width: (cell.width - 4) / 3
                            height: (cell.height - 4) / 3
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                            text: (cell.modelData.notes & (1 << (index + 1))) ? index + 1 : ""
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                            color: Theme.muted
                        }
                    }
                }

                TapHandler {
                    onTapped: board.game.selected = cell.index
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
}
