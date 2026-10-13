import QtQuick
import Calm
import Sudoku

CalmPage {
    id: page

    required property SudokuGame game
    signal settingsRequested()

    function formatTime(seconds) {
        const m = Math.floor(seconds / 60)
        const s = seconds % 60
        return m + ":" + (s < 10 ? "0" : "") + s
    }

    title: game.level
    subtitle: game.showTimer ? formatTime(game.elapsedSeconds) : ""

    actions: IconButton {
        iconPath: Icons.settings
        text: qsTr("Settings")
        onClicked: page.settingsRequested()
    }

    // A tap on the page outside the grid and the controls clears the selection.
    // It sits under the page's own background, which takes no input.
    MouseArea {
        parent: page
        anchors.fill: parent
        z: -1
        onClicked: page.game.clearSelection()
    }

    Column {
        width: parent.width
        spacing: 16
        opacity: page.game.loading ? 0.3 : 1
        Behavior on opacity { NumberAnimation { duration: Theme.gentle } }

        Board {
            width: parent.width
            game: page.game
        }

        Item {
            width: parent.width
            height: controls.height

            // Gaps between controls belong to the controls: a near miss on
            // the pad shouldn't clear the selection.
            MouseArea {
                anchors.fill: parent
            }

            Column {
                id: controls
                width: parent.width
                spacing: 12

                Row {
                    width: parent.width
                    ActionButton {
                        width: parent.width / 4
                        iconPath: Icons.undo
                        text: qsTr("Undo")
                        enabled: page.game.canUndo
                        onClicked: page.game.undo()
                    }
                    ActionButton {
                        width: parent.width / 4
                        iconPath: Icons.erase
                        text: qsTr("Erase")
                        onClicked: page.game.erase()
                    }
                    ActionButton {
                        width: parent.width / 4
                        // Three cells and a tick: several cells at once.
                        iconPath: "M4 4h6v6H4zM14 4h6v6h-6zM4 14h6v6H4zM14 17.5l2.2 2.2L20.5 14.5"
                        text: qsTr("Multi-select")
                        checkable: true
                        checked: page.game.multiSelect
                        onToggled: page.game.multiSelect = checked
                    }
                    ActionButton {
                        width: parent.width / 4
                        iconPath: Icons.hint
                        text: qsTr("Hint")
                        onClicked: page.game.hint()
                    }
                }

                SegmentedControl {
                    width: parent.width
                    height: 44
                    model: [qsTr("Number"), qsTr("Corner"), qsTr("Centre"), qsTr("Colour")]
                    currentIndex: page.game.inputMode
                    Accessible.name: qsTr("Input mode")
                    onActivated: (index) => page.game.inputMode = index
                }

                NumberPad {
                    width: parent.width
                    game: page.game
                }
            }
        }
    }

    CalmText {
        anchors.centerIn: parent
        visible: page.game.loading
        text: qsTr("Preparing a fresh puzzle…")
        color: Theme.muted
    }

    // Solved: a gentle card over the board.
    Rectangle {
        id: solved
        anchors.fill: parent
        color: Theme.ground
        opacity: page.game.complete ? 0.94 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.slow; easing.type: Easing.InOutQuad } }

        TapHandler {} // swallow taps on the board underneath

        Column {
            anchors.centerIn: parent
            width: parent.width
            spacing: 12

            CalmText {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Solved")
                font.pixelSize: 34
                font.weight: Font.ExtraBold
            }
            CalmText {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("%1 in %2").arg(page.game.level).arg(page.formatTime(page.game.elapsedSeconds))
                color: Theme.muted
            }
            Item { width: 1; height: 12 }
            CardButton {
                width: parent.width
                text: qsTr("Another %1 puzzle").arg(page.game.level)
                onClicked: page.game.startNewGame(page.game.level)
                Icon { path: Icons.play; color: Theme.accent }
            }
            CardButton {
                width: parent.width
                text: qsTr("Home")
                fill: Theme.subtle
                onClicked: page.backRequested()
            }
        }
    }
}
