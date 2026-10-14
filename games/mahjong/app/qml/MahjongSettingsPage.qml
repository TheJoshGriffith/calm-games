import QtQuick
import Calm
import Mahjong

SettingsPage {
    id: page

    required property MahjongGame game

    readonly property var themes: ["jade", "classic", "ink", "blossom", "clear"]
    readonly property var themeNames: [qsTr("Jade"), qsTr("Classic"), qsTr("Ink"), qsTr("Blossom"), qsTr("Clear")]

    TilePalette {
        id: preview
        theme: page.game.tileTheme
    }

    Column {
        width: parent.width
        spacing: 8

        CalmText {
            x: 4
            text: qsTr("TILES")
            font.pixelSize: 13
            font.weight: Font.Bold
            font.letterSpacing: 1
            color: Theme.muted
        }

        // A row of sample tiles in the chosen theme.
        Rectangle {
            width: parent.width
            height: 96
            radius: Theme.radius
            color: Theme.surface

            Row {
                anchors.centerIn: parent
                spacing: 8
                Repeater {
                    // 5 of Dots, 3 of Bamboo, 7 of Characters, East, Red dragon, Spring.
                    model: [4, 11, 24, 27, 31, 38]
                    Item {
                        required property int modelData
                        width: 40
                        height: 52
                        Rectangle {
                            x: 3
                            y: 3
                            width: 37
                            height: 49
                            radius: 7
                            color: preview.side
                        }
                        Rectangle {
                            width: 37
                            height: 49
                            radius: 7
                            color: preview.face
                            border.width: 1
                            border.color: preview.edge
                            TileFace {
                                anchors.fill: parent
                                anchors.margins: 5
                                kind: parent.parent.modelData
                                colours: preview
                            }
                        }
                    }
                }
            }
        }

        SegmentedControl {
            width: parent.width
            model: page.themeNames
            currentIndex: Math.max(0, page.themes.indexOf(page.game.tileTheme))
            onActivated: (index) => page.game.tileTheme = page.themes[index]
        }
    }

    SettingsGroup {
        width: parent.width
        title: qsTr("Play")

        ToggleRow {
            text: qsTr("Dim blocked tiles")
            checked: page.game.dimBlocked
            onToggled: page.game.dimBlocked = checked
        }
        ToggleRow {
            text: qsTr("Show timer")
            checked: page.game.showTimer
            onToggled: page.game.showTimer = checked
        }
    }
}
