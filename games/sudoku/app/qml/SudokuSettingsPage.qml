import QtQuick
import Calm
import Sudoku

SettingsPage {
    id: page

    required property SudokuGame game

    SettingsGroup {
        width: parent.width
        title: qsTr("Play")

        ToggleRow {
            text: qsTr("Highlight row, column and box")
            checked: page.game.highlightPeers
            onToggled: page.game.highlightPeers = checked
        }
        ToggleRow {
            text: qsTr("Highlight matching numbers")
            checked: page.game.highlightMatching
            onToggled: page.game.highlightMatching = checked
        }
        ToggleRow {
            text: qsTr("Show mistakes")
            checked: page.game.showMistakes
            onToggled: page.game.showMistakes = checked
        }
        ToggleRow {
            text: qsTr("Show timer")
            checked: page.game.showTimer
            onToggled: page.game.showTimer = checked
        }
        ToggleRow {
            text: qsTr("Auto-remove notes")
            checked: page.game.autoRemoveNotes
            onToggled: page.game.autoRemoveNotes = checked
        }
    }
}
