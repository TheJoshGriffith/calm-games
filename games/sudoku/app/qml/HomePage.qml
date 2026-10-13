import QtQuick
import Calm
import Sudoku

Item {
    id: page

    required property SudokuGame game
    signal playRequested()
    signal settingsRequested()

    readonly property var levels: [
        { name: "Easy", hint: qsTr("Singles only"), dots: 1 },
        { name: "Medium", hint: qsTr("Pairs and pointing"), dots: 2 },
        { name: "Hard", hint: qsTr("Triples and X-Wing"), dots: 3 },
        { name: "Expert", hint: qsTr("Wings and swordfish"), dots: 4 }
    ]

    function formatTime(seconds) {
        const m = Math.floor(seconds / 60)
        const s = seconds % 60
        return m + ":" + (s < 10 ? "0" : "") + s
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.ground
    }

    Column {
        anchors.fill: parent
        anchors.topMargin: Theme.safeTop + 24
        anchors.leftMargin: 24
        anchors.rightMargin: 24
        spacing: 28

        Column {
            width: parent.width
            spacing: 6
            CalmText {
                text: qsTr("Sudoku")
                font.pixelSize: 40
                font.weight: Font.ExtraBold
                Accessible.role: Accessible.Heading
            }
            CalmText {
                text: qsTr("Endless puzzles. No ads, ever.")
                color: Theme.muted
            }
        }

        CardButton {
            width: parent.width
            visible: page.game.hasSavedGame || (page.game.active && !page.game.complete)
            text: qsTr("Continue")
            subtitle: page.game.active
                      ? page.game.level + " · " + page.formatTime(page.game.elapsedSeconds)
                      : qsTr("Pick up where you left off")
            onClicked: {
                if (page.game.active || page.game.resume())
                    page.playRequested()
            }
            Icon { path: Icons.play; color: Theme.accent }
        }

        Column {
            width: parent.width
            spacing: 12

            CalmText {
                x: 4
                text: qsTr("NEW GAME")
                font.pixelSize: 15
                font.weight: Font.Bold
                font.letterSpacing: 1
                color: Theme.muted
            }

            Repeater {
                model: page.levels

                CardButton {
                    required property var modelData
                    width: parent.width
                    text: qsTr(modelData.name)
                    subtitle: modelData.hint
                    onClicked: {
                        page.game.startNewGame(modelData.name)
                        page.playRequested()
                    }

                    Row {
                        spacing: 5
                        Accessible.ignored: true
                        Repeater {
                            model: 4
                            Rectangle {
                                required property int index
                                width: 8
                                height: 8
                                radius: 4
                                color: index < modelData.dots ? Theme.accent : Theme.line
                            }
                        }
                    }
                }
            }
        }
    }

    IconButton {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 24
        iconPath: Icons.settings
        text: qsTr("Settings")
        onClicked: page.settingsRequested()
    }
}
