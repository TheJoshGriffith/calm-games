import QtQuick
import Calm
import Minesweeper

// The minefield. Tap to open, long press (or right click) to flag. Opening an
// area ripples outwards from the tapped cell.
Item {
    id: board

    required property MinesweeperGame game
    readonly property int columns: game.columns
    readonly property int rows: game.rows
    readonly property real cellSize: columns > 0 ? Math.floor(Math.min(width / columns, height / rows)) : 0
    // Read once per change; delegates index into it, so they persist and animate.
    readonly property var cells: game.cells

    // Stroke icons on the 24x24 Icon grid.
    readonly property string flagPath: "M7 21V4M7 4h10l-2.5 4 2.5 4H7"
    readonly property string minePath: "M12 7.5a4.5 4.5 0 1 0 0 9a4.5 4.5 0 1 0 0-9M12 3v2.5M12 18.5V21M3 12h2.5M18.5 12H21M5.6 5.6l1.8 1.8M16.6 16.6l1.8 1.8M5.6 18.4l1.8-1.8M16.6 7.4l1.8-1.8"

    function numberColor(n) {
        return n === 1 ? Theme.accent : n === 2 ? Theme.entry : n === 3 ? Theme.danger : Theme.ink
    }

    Grid {
        anchors.centerIn: parent
        columns: board.columns

        Repeater {
            model: board.columns * board.rows

            Item {
                id: cell

                required property int index
                readonly property int view: index < board.cells.length ? board.cells[index] : MinesweeperGame.Hidden
                readonly property bool open: view >= 0 && view <= 8
                // Distance from where the last reveal started, for the ripple.
                readonly property real delay: {
                    if (Theme.quick === 0 || board.game.origin < 0)
                        return 0
                    const dr = Math.floor(index / board.columns) - Math.floor(board.game.origin / board.columns)
                    const dc = index % board.columns - board.game.origin % board.columns
                    return Math.min(Math.sqrt(dr * dr + dc * dc) * 22, 400)
                }

                width: board.cellSize
                height: board.cellSize

                Accessible.role: Accessible.Button
                Accessible.name: qsTr("Row %1, column %2, %3")
                                 .arg(Math.floor(index / board.columns) + 1).arg(index % board.columns + 1)
                                 .arg(view === MinesweeperGame.Flagged ? qsTr("flagged")
                                      : view === MinesweeperGame.Hidden ? qsTr("hidden")
                                      : view === 0 ? qsTr("clear")
                                      : view <= 8 ? view : qsTr("mine"))

                Rectangle {
                    id: tile
                    anchors.fill: parent
                    anchors.margins: Math.max(1, board.cellSize * 0.05)
                    radius: Math.min(Theme.radiusSmall, board.cellSize * 0.24)
                    color: cell.view === MinesweeperGame.Exploded ? Theme.danger
                         : cell.open ? Theme.surface
                         : Theme.subtle

                    Behavior on color {
                        SequentialAnimation {
                            PauseAnimation { duration: cell.delay }
                            ColorAnimation { duration: Theme.gentle; easing.type: Easing.OutQuad }
                        }
                    }
                }

                CalmText {
                    anchors.centerIn: parent
                    visible: cell.view >= 1 && cell.view <= 8
                    text: cell.view
                    font.pixelSize: Math.round(board.cellSize * 0.52)
                    font.weight: Font.ExtraBold
                    color: board.numberColor(cell.view)
                    opacity: visible ? 1 : 0
                    Behavior on opacity {
                        SequentialAnimation {
                            PauseAnimation { duration: cell.delay }
                            NumberAnimation { duration: Theme.gentle }
                        }
                    }
                }

                Icon {
                    anchors.centerIn: parent
                    visible: cell.view === MinesweeperGame.Flagged || cell.view === MinesweeperGame.WrongFlag
                             || cell.view === MinesweeperGame.Mine || cell.view === MinesweeperGame.Exploded
                    size: board.cellSize * 0.62
                    strokeWidth: 2
                    path: cell.view === MinesweeperGame.Mine || cell.view === MinesweeperGame.Exploded
                          ? board.minePath : board.flagPath
                    color: cell.view === MinesweeperGame.Exploded ? Theme.surface
                         : cell.view === MinesweeperGame.WrongFlag ? Theme.danger
                         : cell.view === MinesweeperGame.Mine ? Theme.muted
                         : Theme.accent
                    scale: visible ? 1 : 0.6
                    Behavior on scale { NumberAnimation { duration: Theme.quick; easing.type: Easing.OutBack } }
                }

                TapHandler {
                    acceptedButtons: Qt.LeftButton
                    onTapped: board.game.tap(cell.index)
                    onLongPressed: board.game.flag(cell.index)
                }
                TapHandler {
                    acceptedButtons: Qt.RightButton
                    onTapped: board.game.flag(cell.index)
                }
            }
        }
    }
}
