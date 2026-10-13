import QtQuick
import Calm
import Sudoku

Item {
    id: page

    required property SudokuGame game
    signal playRequested()
    signal settingsRequested()
    signal statsRequested()

    readonly property var levels: [
        { name: "Easy", hint: qsTr("Singles only"), dots: 1 },
        { name: "Medium", hint: qsTr("Pairs and pointing"), dots: 2 },
        { name: "Hard", hint: qsTr("Triples and X-Wing"), dots: 3 },
        { name: "Expert", hint: qsTr("Wings and swordfish"), dots: 4 }
    ]

    // Why the last code typed could not be opened, or "".
    property string codeError: ""

    function formatTime(seconds) {
        const m = Math.floor(seconds / 60)
        const s = seconds % 60
        return m + ":" + (s < 10 ? "0" : "") + s
    }

    // Opens the code sheet; with `text`, types it in and tries it.
    function openCodeSheet(text) {
        codeError = ""
        codeField.text = text || ""
        codeSheet.open()
        if (text)
            submitCode()
        else
            codeField.forceActiveFocus()
    }

    // A puzzle code, or a whole share link pasted in.
    function submitCode() {
        const text = codeField.text.trim()
        if (text.length === 0) {
            codeError = qsTr("Type the code your friend sent you.")
            return
        }
        if (page.game.startFromCode(text) || (text.indexOf("/") >= 0 && page.game.openShared(text))) {
            codeSheet.close()
            codeField.text = ""
            codeError = ""
            page.playRequested()
            return
        }
        codeError = qsTr("That code doesn't look right. Codes look like H1-3KX9QZ0A.")
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.ground
    }

    Flickable {
        anchors.fill: parent
        contentHeight: content.height + Theme.safeTop + 24 + 24 + Theme.touch + 24
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: content
            x: 24
            y: Theme.safeTop + 24
            width: page.width - 48
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

                CardButton {
                    width: parent.width
                    text: qsTr("Enter a puzzle code")
                    subtitle: qsTr("Play the puzzle a friend sent you")
                    fill: Theme.subtle
                    onClicked: page.openCodeSheet()
                    Icon { path: Icons.code; color: Theme.accent }
                }
            }
        }
    }

    Row {
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 24
        spacing: 8

        IconButton {
            iconPath: Icons.stats
            text: qsTr("Statistics")
            onClicked: page.statsRequested()
        }
        IconButton {
            iconPath: Icons.settings
            text: qsTr("Settings")
            onClicked: page.settingsRequested()
        }
    }

    BottomSheet {
        id: codeSheet
        title: qsTr("Enter a puzzle code")
        onClosed: page.codeError = ""

        CalmText {
            width: parent.width
            text: qsTr("Every puzzle has a code, under Share in the game. Type one in to play the very same puzzle.")
            wrapMode: Text.WordWrap
            font.pixelSize: 15
            color: Theme.muted
        }
        CalmTextField {
            id: codeField
            width: parent.width
            placeholderText: qsTr("Code, like H1-3KX9QZ0A")
            error: page.codeError.length > 0
            inputMethodHints: Qt.ImhNoPredictiveText | Qt.ImhPreferUppercase
            onTextEdited: page.codeError = ""
            onAccepted: page.submitCode()
        }
        CalmText {
            width: parent.width
            visible: page.codeError.length > 0
            text: page.codeError
            wrapMode: Text.WordWrap
            font.pixelSize: 14
            font.weight: Font.DemiBold
            color: Theme.danger
            Accessible.role: Accessible.AlertMessage
            Accessible.name: text
        }
        CardButton {
            width: parent.width
            text: qsTr("Play this puzzle")
            onClicked: page.submitCode()
            Icon { path: Icons.play; color: Theme.accent }
        }
    }
}
