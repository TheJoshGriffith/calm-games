import QtQuick
import QtQuick.Templates as T
import Calm
import Sudoku

// A 3x3 pad echoing the board. In Number mode each key shows how many of that
// digit remain; Corner and Centre modes preview where the mark will go; in
// Colour mode it becomes nine swatches and a clear-colour key. The pad keeps
// the same height in every mode so nothing below it jumps.
Column {
    id: pad

    required property SudokuGame game

    readonly property int mode: game.inputMode
    readonly property bool colourMode: mode === SudokuGame.ColourMode
    readonly property int keyHeight: 52
    // Four rows (swatches plus the clear key) in the space of three.
    readonly property real swatchHeight: (3 * keyHeight - spacing) / 4

    spacing: 8

    Grid {
        width: pad.width
        columns: 3
        spacing: pad.spacing

        Repeater {
            model: 9

            T.AbstractButton {
                id: key

                required property int index
                readonly property int digit: index + 1
                readonly property int remainingCount: pad.game.remaining.length === 9 ? pad.game.remaining[index] : 9

                width: (pad.width - 2 * pad.spacing) / 3
                height: pad.colourMode ? pad.swatchHeight : pad.keyHeight
                text: digit
                enabled: pad.colourMode || pad.mode !== SudokuGame.NumberMode || remainingCount > 0
                Accessible.name: pad.colourMode ? CellColours.names[index]
                               : pad.mode === SudokuGame.CornerMode ? qsTr("Corner mark %1").arg(digit)
                               : pad.mode === SudokuGame.CentreMode ? qsTr("Centre mark %1").arg(digit)
                               : qsTr("%1, %2 left").arg(digit).arg(remainingCount)
                onClicked: pad.colourMode ? pad.game.paint(digit) : pad.game.enter(digit)

                contentItem: Item {
                    visible: !pad.colourMode
                    CalmText {
                        // Number: large and centred. Corner: small in the
                        // top-left. Centre: small in the middle.
                        x: pad.mode === SudokuGame.CornerMode ? 12 : (parent.width - width) / 2
                        y: pad.mode === SudokuGame.CornerMode ? 6 : (parent.height - height) / 2
                        text: key.digit
                        font.pixelSize: pad.mode === SudokuGame.NumberMode ? 24 : 17
                        font.weight: pad.mode === SudokuGame.NumberMode ? Font.DemiBold : Font.Bold
                        color: pad.mode === SudokuGame.NumberMode ? Theme.ink : Theme.muted
                    }
                    CalmText {
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        anchors.rightMargin: 12
                        anchors.bottomMargin: 7
                        visible: pad.mode === SudokuGame.NumberMode
                        text: key.remainingCount
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        color: Theme.muted
                    }
                }

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: pad.colourMode ? CellColours.colour(key.digit)
                         : key.down ? Theme.subtle : Theme.raised
                    border.color: Theme.line
                    border.width: pad.colourMode ? 1 : 0
                    opacity: key.enabled ? (pad.colourMode && key.down ? 0.7 : 1) : 0.35
                    Behavior on color { ColorAnimation { duration: Theme.quick } }
                }
            }
        }
    }

    T.AbstractButton {
        id: clearKey

        width: pad.width
        height: pad.swatchHeight
        visible: pad.colourMode
        text: qsTr("Clear colour")
        Accessible.name: text
        onClicked: pad.game.paint(0)

        contentItem: Item {
            Row {
                anchors.centerIn: parent
                spacing: 8
                Icon {
                    anchors.verticalCenter: parent.verticalCenter
                    path: Icons.close
                    size: 18
                    color: Theme.muted
                }
                CalmText {
                    anchors.verticalCenter: parent.verticalCenter
                    text: clearKey.text
                    font.pixelSize: 15
                    font.weight: Font.Bold
                    color: Theme.muted
                }
            }
        }

        background: Rectangle {
            radius: Theme.radiusSmall
            color: clearKey.down ? Theme.subtle : Theme.raised
            Behavior on color { ColorAnimation { duration: Theme.quick } }
        }
    }
}
