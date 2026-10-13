import QtQuick
import Calm
import Minesweeper

CalmPage {
    id: page

    required property MinesweeperGame game
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

    // Mines still to find.
    Row {
        id: counter
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        spacing: 8
        Icon {
            anchors.verticalCenter: parent.verticalCenter
            path: "M7 21V4M7 4h10l-2.5 4 2.5 4H7"
            color: Theme.accent
            size: 20
        }
        CalmText {
            anchors.verticalCenter: parent.verticalCenter
            text: page.game.minesLeft === 1 ? qsTr("1 mine left")
                                            : qsTr("%1 mines left").arg(Math.max(0, page.game.minesLeft))
            font.weight: Font.Bold
            color: Theme.muted
        }
    }

    Board {
        id: board
        anchors.top: counter.bottom
        anchors.topMargin: 12
        anchors.bottom: toolbar.top
        anchors.bottomMargin: 12
        anchors.left: parent.left
        anchors.right: parent.right
        game: page.game
    }

    Row {
        id: toolbar
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        ActionButton {
            width: parent.width / 3
            iconPath: Icons.undo
            text: qsTr("Undo")
            enabled: page.game.canUndo
            onClicked: page.game.undo()
        }
        ActionButton {
            width: parent.width / 3
            iconPath: board.flagPath
            text: page.game.flagMode ? qsTr("Flagging") : qsTr("Flag")
            checkable: true
            checked: page.game.flagMode
            onToggled: page.game.flagMode = checked
        }
        ActionButton {
            width: parent.width / 3
            iconPath: Icons.hint
            text: qsTr("Hint")
            onClicked: page.game.hint()
        }
    }

    // Stepped on a mine: a gentle card over the toolbar, with the board still
    // visible. Taking it back is just undo.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: lostColumn.implicitHeight + 32
        radius: Theme.radius
        color: Theme.raised
        border.color: Theme.line
        opacity: page.game.lost ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.gentle } }

        TapHandler {} // swallow taps on the toolbar underneath

        Column {
            id: lostColumn
            anchors.centerIn: parent
            width: parent.width - 32
            spacing: 12

            CalmText {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("That one was a mine")
                font.pixelSize: 20
                font.weight: Font.ExtraBold
            }
            Row {
                width: parent.width
                spacing: 12
                CardButton {
                    width: (parent.width - 12) / 2
                    text: qsTr("Take it back")
                    onClicked: page.game.undo()
                }
                CardButton {
                    width: (parent.width - 12) / 2
                    text: qsTr("New board")
                    fill: Theme.subtle
                    onClicked: page.game.startNewGame(page.game.level)
                }
            }
        }
    }

    // Cleared: a gentle card over the board.
    Rectangle {
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
                text: qsTr("Cleared")
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
                text: qsTr("Another %1 board").arg(page.game.level)
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
