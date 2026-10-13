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
        Item {
            width: parent.width
            height: mistakesControl.y + mistakesControl.height + 6 + mistakesHelp.height + 12
            CalmText {
                id: mistakesLabel
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: mistakesControl.verticalCenter
                text: qsTr("Mistakes")
                font.weight: Font.DemiBold
            }
            SegmentedControl {
                id: mistakesControl
                anchors.left: mistakesLabel.right
                anchors.right: parent.right
                anchors.leftMargin: 16
                anchors.rightMargin: 8
                y: 8
                height: 44
                color: Theme.ground
                model: [qsTr("Off"), qsTr("Clashes"), qsTr("Answer")]
                currentIndex: page.game.mistakeMode
                Accessible.name: qsTr("Show mistakes")
                onActivated: (index) => page.game.mistakeMode = index
            }
            CalmText {
                id: mistakesHelp
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                anchors.top: mistakesControl.bottom
                anchors.topMargin: 6
                wrapMode: Text.WordWrap
                font.pixelSize: 13
                color: Theme.muted
                text: [qsTr("Mistakes aren't marked."),
                       qsTr("Marks a number that repeats in its row, column or box."),
                       qsTr("Marks any number that differs from the answer.")][page.game.mistakeMode]
            }
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
