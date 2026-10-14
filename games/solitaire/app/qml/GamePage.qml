import QtQuick
import Calm
import Solitaire

CalmPage {
    id: page

    required property SolitaireGame game
    signal settingsRequested()

    // A shuffle mark for "New deal", on the Icons 24x24 stroke grid.
    readonly property string shuffleIcon: "M3 7h3.5c2 0 3 1 4.2 2.6l2.6 4.8c1.2 1.6 2.2 2.6 4.2 2.6H21M3 17h3.5c1.4 0 2.3-.5 3.2-1.4M14.3 8.4c.9-.9 1.8-1.4 3.2-1.4H21M18 4l3 3-3 3M18 14l3 3-3 3"

    function formatTime(seconds) {
        const m = Math.floor(seconds / 60)
        const s = seconds % 60
        return m + ":" + (s < 10 ? "0" : "") + s
    }

    title: game.level
    subtitle: (game.showTimer ? formatTime(game.elapsedSeconds) + "  ·  " : "")
              + (game.moves === 1 ? qsTr("1 move") : qsTr("%1 moves").arg(game.moves))

    actions: IconButton {
        iconPath: Icons.settings
        text: qsTr("Settings")
        onClicked: page.settingsRequested()
    }

    Table {
        id: table
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: toolbar.top
        anchors.bottomMargin: 8
        game: page.game
        opacity: page.game.loading ? 0.3 : 1
        Behavior on opacity { NumberAnimation { duration: Theme.gentle } }
    }

    Row {
        id: toolbar
        anchors.bottom: parent.bottom
        width: parent.width

        ActionButton {
            width: parent.width / 3
            iconPath: Icons.undo
            text: qsTr("Undo")
            enabled: page.game.canUndo && !page.game.autoFinishing
            onClicked: page.game.undo()
        }
        ActionButton {
            width: parent.width / 3
            iconPath: Icons.hint
            text: qsTr("Hint")
            enabled: page.game.active && !page.game.autoFinishing
            onClicked: {
                if (!page.game.showHint())
                    noMoves.shown = true
            }
        }
        ActionButton {
            width: parent.width / 3
            iconPath: page.shuffleIcon
            text: qsTr("New deal")
            enabled: !page.game.loading
            onClicked: page.game.startNewGame(page.game.level)
        }
    }

    // Every card is face up: offer to play the rest home.
    CardButton {
        id: autoComplete

        readonly property bool offered: page.game.autoCompleteButton && page.game.canAutoFinish
                                        && !page.game.autoFinishing && !page.game.losingMove

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: toolbar.top
        anchors.bottomMargin: 12
        width: Math.min(parent.width, 280)
        text: qsTr("Auto-complete")
        subtitle: qsTr("Play the remaining cards home")
        enabled: offered
        opacity: offered ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.gentle } }
        onClicked: page.game.autoFinish()
        Icon { path: Icons.play; color: Theme.accent }
    }

    // The solver found that the last move lost the deal.
    Rectangle {
        id: losing

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: toolbar.top
        anchors.bottomMargin: 12
        width: parent.width
        height: losingRow.implicitHeight + 24
        radius: Theme.radius
        color: Theme.surface
        border.color: Theme.danger
        opacity: page.game.losingMove ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.gentle } }

        Accessible.role: Accessible.AlertMessage
        Accessible.name: losingText.text

        Row {
            id: losingRow
            anchors.verticalCenter: parent.verticalCenter
            x: 16
            width: parent.width - 24
            spacing: 8

            CalmText {
                id: losingText
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - undoLosing.width - parent.spacing
                text: qsTr("That move means this deal can't be won any more.")
                wrapMode: Text.WordWrap
                font.pixelSize: 14
            }
            ActionButton {
                id: undoLosing
                anchors.verticalCenter: parent.verticalCenter
                iconPath: Icons.undo
                text: qsTr("Undo")
                enabled: page.game.losingMove
                onClicked: page.game.undo()
            }
        }
    }

    // "No helpful moves" note, shown briefly after an empty hint.
    Rectangle {
        id: noMoves

        property bool shown: false

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: toolbar.top
        anchors.bottomMargin: 16
        width: noMovesText.implicitWidth + 32
        height: 40
        radius: 20
        color: Theme.surface
        border.color: Theme.line
        opacity: shown ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.gentle } }

        CalmText {
            id: noMovesText
            anchors.centerIn: parent
            text: qsTr("No helpful moves left. Try a new deal.")
            font.pixelSize: 14
            color: Theme.muted
        }

        Timer {
            running: noMoves.shown
            interval: 2600
            onTriggered: noMoves.shown = false
        }

        Connections {
            target: page.game
            function onStateChanged() { noMoves.shown = false }
        }
    }

    CalmText {
        anchors.centerIn: parent
        visible: page.game.loading
        text: page.game.winnableDeals && !page.game.level.startsWith("Spider")
              ? qsTr("Shuffling a winnable deal…") : qsTr("Shuffling…")
        color: Theme.muted
    }

    // Won: a gentle card over the table.
    Rectangle {
        anchors.fill: parent
        color: Theme.ground
        opacity: page.game.complete ? 0.94 : 0
        visible: opacity > 0
        Behavior on opacity {
            SequentialAnimation {
                // Let the last cards land first.
                PauseAnimation { duration: Theme.gentle }
                NumberAnimation { duration: Theme.slow; easing.type: Easing.InOutQuad }
            }
        }

        TapHandler {} // swallow taps on the table underneath

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
                text: qsTr("%1 moves in %2").arg(page.game.moves).arg(page.formatTime(page.game.elapsedSeconds))
                color: Theme.muted
            }
            Item { width: 1; height: 12 }
            CardButton {
                width: parent.width
                text: qsTr("Deal again")
                subtitle: page.game.level
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
