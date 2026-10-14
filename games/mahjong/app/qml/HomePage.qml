import QtQuick
import Calm
import Mahjong

Item {
    id: page

    required property MahjongGame game
    signal playRequested()
    signal settingsRequested()
    signal statsRequested()

    function formatTime(seconds) {
        const m = Math.floor(seconds / 60)
        const s = seconds % 60
        return m + ":" + (s < 10 ? "0" : "") + s
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.ground
    }

    Flickable {
        anchors.fill: parent
        contentHeight: column.implicitHeight + Theme.safeTop + 24 + 96
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Column {
            id: column
            y: Theme.safeTop + 24
            x: 24
            width: parent.width - 48
            spacing: 28

            Column {
                width: parent.width
                spacing: 6
                CalmText {
                    text: qsTr("Mahjong")
                    font.pixelSize: 40
                    font.weight: Font.ExtraBold
                    Accessible.role: Accessible.Heading
                }
                CalmText {
                    width: parent.width
                    text: qsTr("Every deal can be cleared. No ads, ever.")
                    wrapMode: Text.WordWrap
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
                    model: page.game.layouts

                    CardButton {
                        required property var modelData
                        width: parent.width
                        text: qsTr(modelData.name)
                        subtitle: qsTr("%1 tiles · %2 layers").arg(modelData.tiles).arg(modelData.layers)
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
                                    color: index < modelData.difficulty ? Theme.accent : Theme.line
                                }
                            }
                        }
                    }
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
}
