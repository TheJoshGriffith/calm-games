import QtQuick
import QtQuick.Templates as T
import Calm
import Mahjong

CalmPage {
    id: page

    required property MahjongGame game
    signal settingsRequested()
    // A short note for the window to show ("Code copied").
    signal noteRequested(string message)

    objectName: "gamePage"

    function openShareSheet() { shareSheet.open() }

    // Shares a link to this deal through the system share sheet (or the
    // clipboard, where there is none).
    function share() {
        const url = page.game.shareUrl()
        if (url.length === 0)
            return
        const p = shareButton.mapToItem(null, 0, 0)
        shareSheet.close()
        Sharing.share(qsTr("Try this %1 Mahjong deal: %2").arg(page.game.level).arg(url),
                      Qt.rect(p.x, p.y, shareButton.width, shareButton.height))
    }

    function formatTime(seconds) {
        const m = Math.floor(seconds / 60)
        const s = seconds % 60
        return m + ":" + (s < 10 ? "0" : "") + s
    }

    title: game.level
    subtitle: game.showTimer ? formatTime(game.elapsedSeconds) : ""

    actions: [
        IconButton {
            id: shareButton
            iconPath: Icons.share
            text: qsTr("Share")
            enabled: page.game.active && page.game.dealCode.length > 0
            onClicked: shareSheet.open()
        },
        IconButton {
            iconPath: Icons.settings
            text: qsTr("Settings")
            onClicked: page.settingsRequested()
        }
    ]

    // Keyboard (desktop and the web): H for a hint, S to shuffle, Ctrl+Z / Ctrl+Y.
    readonly property bool keysEnabled: T.StackView.status === T.StackView.Active && !shareSheet.opened
    Shortcut { sequence: "H"; enabled: page.keysEnabled; onActivated: page.game.hint() }
    Shortcut { sequence: "S"; enabled: page.keysEnabled; onActivated: page.game.shuffle() }
    Shortcut { sequences: [StandardKey.Undo]; enabled: page.keysEnabled; onActivated: page.game.undo() }
    Shortcut { sequences: [StandardKey.Redo]; enabled: page.keysEnabled; onActivated: page.game.redo() }

    // Tiles and pairs left.
    CalmText {
        id: counter
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        text: page.game.movesAvailable === 1
              ? qsTr("%1 tiles · 1 match open").arg(page.game.tilesLeft)
              : qsTr("%1 tiles · %2 matches open").arg(page.game.tilesLeft).arg(page.game.movesAvailable)
        font.weight: Font.Bold
        color: Theme.muted
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
            iconPath: Icons.hint
            text: qsTr("Hint")
            onClicked: page.game.hint()
        }
        ActionButton {
            width: parent.width / 3
            // Two curved arrows chasing each other.
            iconPath: "M4 10a8 8 0 0 1 14-4.5M18 2v4h-4M20 14a8 8 0 0 1-14 4.5M6 22v-4h4"
            text: qsTr("Shuffle")
            onClicked: {
                if (!page.game.shuffle())
                    page.noteRequested(qsTr("These tiles can't be cleared now. Try undo."))
            }
        }
    }

    // No pairs left to take: a gentle card over the toolbar.
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: stuckColumn.implicitHeight + 32
        radius: Theme.radius
        color: Theme.raised
        border.color: Theme.line
        opacity: page.game.stuck ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.gentle } }

        TapHandler {} // swallow taps on the toolbar underneath

        Column {
            id: stuckColumn
            anchors.centerIn: parent
            width: parent.width - 32
            spacing: 12

            CalmText {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("No more matches")
                font.pixelSize: 20
                font.weight: Font.ExtraBold
            }
            Row {
                width: parent.width
                spacing: 12
                CardButton {
                    width: (parent.width - 12) / 2
                    text: qsTr("Shuffle")
                    onClicked: {
                        if (!page.game.shuffle())
                            page.noteRequested(qsTr("These tiles can't be cleared now. Try undo."))
                    }
                }
                CardButton {
                    width: (parent.width - 12) / 2
                    text: qsTr("Undo")
                    fill: Theme.subtle
                    enabled: page.game.canUndo
                    onClicked: page.game.undo()
                }
            }
        }
    }

    // Cleared: a gentle card over the table.
    Rectangle {
        anchors.fill: parent
        color: Theme.ground
        opacity: page.game.complete ? 0.94 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.slow; easing.type: Easing.InOutQuad } }

        TapHandler {} // swallow taps on the table underneath

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
                text: qsTr("Another %1 deal").arg(page.game.level)
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

    // Share: the deal code to read out or copy, and a link to the deal.
    BottomSheet {
        id: shareSheet
        parent: page
        title: qsTr("Share deal")

        CardButton {
            width: parent.width
            text: page.game.dealCode
            subtitle: qsTr("Deal code · tap to copy")
            fill: Theme.surface
            onClicked: {
                Sharing.copy(page.game.dealCode)
                page.noteRequested(qsTr("Code copied"))
            }
            Icon { path: Icons.copy; color: Theme.accent }
        }
        CardButton {
            width: parent.width
            text: qsTr("Share this deal")
            subtitle: qsTr("Friends get the same tiles, from the start")
            onClicked: page.share()
            Icon { path: Icons.share; color: Theme.accent }
        }
    }
}
