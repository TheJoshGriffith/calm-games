import QtQuick
import Calm
import Solitaire

SettingsPage {
    id: page

    required property SolitaireGame game

    SettingsGroup {
        width: parent.width
        title: qsTr("Play")

        ToggleRow {
            text: qsTr("Only winnable deals")
            checked: page.game.winnableDeals
            onToggled: page.game.winnableDeals = checked
        }
        ToggleRow {
            text: qsTr("Auto-complete button")
            checked: page.game.autoCompleteButton
            onToggled: page.game.autoCompleteButton = checked
        }
        ToggleRow {
            text: qsTr("Warn about losing moves")
            checked: page.game.warnLosingMoves
            onToggled: page.game.warnLosingMoves = checked
        }
        ToggleRow {
            text: qsTr("Show timer")
            checked: page.game.showTimer
            onToggled: page.game.showTimer = checked
        }
    }
}
