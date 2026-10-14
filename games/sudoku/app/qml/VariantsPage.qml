import QtQuick
import Calm
import Sudoku

// The variant picker: each variant as a card with a one-line summary, in two
// groups (extra rules, clues on the grid). Tapping one starts a fresh puzzle.
CalmPage {
    id: page

    required property SudokuGame game
    signal playRequested()

    title: qsTr("Variants")

    Flickable {
        anchors.fill: parent
        contentHeight: column.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Column {
            id: column
            width: parent.width
            spacing: 24

            CalmText {
                width: parent.width
                text: qsTr("Sudoku with a twist. The usual rules still hold, plus one more.")
                wrapMode: Text.WordWrap
                color: Theme.muted
            }

            Repeater {
                model: Variants.groups

                Column {
                    id: group
                    required property var modelData
                    width: column.width
                    spacing: 12

                    CalmText {
                        x: 4
                        text: group.modelData.title.toUpperCase()
                        font.pixelSize: 15
                        font.weight: Font.Bold
                        font.letterSpacing: 1
                        color: Theme.muted
                        Accessible.role: Accessible.Heading
                    }

                    Repeater {
                        model: group.modelData.ids

                        CardButton {
                            required property string modelData
                            width: group.width
                            text: Variants.info[modelData].name
                            subtitle: Variants.info[modelData].summary
                            onClicked: {
                                page.game.startNewGame(page.game.variantLevel(modelData))
                                page.playRequested()
                            }
                            Icon { path: Icons.play; color: Theme.accent; size: 18 }
                        }
                    }
                }
            }

            Item { width: 1; height: 8 }
        }
    }
}
