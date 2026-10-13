import QtQuick
import QtQuick.Templates as T
import Calm
import Sudoku

// A 3x3 pad echoing the board; each key shows how many of that digit remain.
Grid {
    id: pad

    required property SudokuGame game

    columns: 3
    spacing: 8

    Repeater {
        model: 9

        T.AbstractButton {
            id: key

            required property int index
            readonly property int digit: index + 1
            readonly property int remainingCount: pad.game.remaining.length === 9 ? pad.game.remaining[index] : 9

            width: (pad.width - 2 * pad.spacing) / 3
            height: 52
            text: digit
            enabled: remainingCount > 0 || pad.game.notesMode
            Accessible.name: qsTr("%1, %2 left").arg(digit).arg(remainingCount)
            onClicked: pad.game.enter(digit)

            contentItem: Item {
                CalmText {
                    anchors.centerIn: parent
                    text: key.digit
                    font.pixelSize: 24
                    font.weight: Font.DemiBold
                    color: pad.game.notesMode ? Theme.muted : Theme.ink
                }
                CalmText {
                    anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    anchors.rightMargin: 12
                    anchors.bottomMargin: 7
                    text: key.remainingCount
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                    color: Theme.muted
                }
            }

            background: Rectangle {
                radius: Theme.radiusSmall
                color: key.down ? Theme.subtle : Theme.raised
                opacity: key.enabled ? 1 : 0.35
                Behavior on color { ColorAnimation { duration: Theme.quick } }
            }
        }
    }
}
