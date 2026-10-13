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
            text: qsTr("Tap a card to move it")
            checked: page.game.tapToMove
            onToggled: page.game.tapToMove = checked
        }
        ToggleRow {
            text: qsTr("Show timer")
            checked: page.game.showTimer
            onToggled: page.game.showTimer = checked
        }
    }
}
