import QtQuick
import Calm
import Minesweeper

SettingsPage {
    id: page

    required property MinesweeperGame game

    SettingsGroup {
        width: parent.width
        title: qsTr("Play")

        ToggleRow {
            text: qsTr("No guessing needed")
            checked: page.game.noGuessing
            onToggled: page.game.noGuessing = checked
        }
        ToggleRow {
            text: qsTr("Show timer")
            checked: page.game.showTimer
            onToggled: page.game.showTimer = checked
        }
    }
}
