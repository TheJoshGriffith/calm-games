import QtQuick
import Calm
import Pinball

Item {
    id: page

    required property PinballGame game
    signal playRequested()
    signal settingsRequested()

    readonly property var levels: [
        { name: "Gentle", hint: qsTr("Endless balls, stage after stage"), dots: 1 },
        { name: "Classic", hint: qsTr("Three balls. Clear stages for more"), dots: 2 }
    ]

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
                text: qsTr("Pinball")
                font.pixelSize: 40
                font.weight: Font.ExtraBold
                Accessible.role: Accessible.Heading
            }
            CalmText {
                text: qsTr("A quiet table. No ads, ever.")
                color: Theme.muted
            }
        }

        CardButton {
            width: parent.width
            visible: page.game.hasSavedGame || (page.game.active && !page.game.complete)
            text: qsTr("Continue")
            subtitle: page.game.active
                      ? page.game.level + " · " + Number(page.game.score).toLocaleString(Qt.locale(), "f", 0)
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
                    readonly property real best: {
                        page.game.score // re-read after each game
                        return page.game.bestFor(modelData.name)
                    }
                    width: parent.width
                    text: qsTr(modelData.name)
                    subtitle: best > 0
                              ? modelData.hint + "\n" + qsTr("Best %1").arg(Number(best).toLocaleString(Qt.locale(), "f", 0))
                              : modelData.hint
                    onClicked: {
                        page.game.startNewGame(modelData.name)
                        page.playRequested()
                    }

                    Row {
                        spacing: 5
                        Accessible.ignored: true
                        Repeater {
                            model: 2
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
