import QtQuick
import QtQuick.Templates as T
import Calm
import Solitaire

Item {
    id: page

    required property SolitaireGame game
    signal playRequested()
    signal settingsRequested()

    // Klondike and Spider each have a choice, picked with the small
    // buttons on their card: cards per turn, and suits.
    readonly property var games: [
        { key: "klondike", name: qsTr("Klondike"), choices: [1, 3],
          hints: [qsTr("The classic, turning one card at a time"), qsTr("The classic, turning three at a time")] },
        { key: "freecell", name: qsTr("FreeCell"), choices: [],
          hints: [qsTr("Every card face up, four cells to work with")] },
        { key: "spider", name: qsTr("Spider"), choices: [1, 2, 4],
          hints: [qsTr("Two decks in one suit"), qsTr("Two decks in two suits"), qsTr("Two decks in four suits")] },
        { key: "yukon", name: qsTr("Yukon"), choices: [],
          hints: [qsTr("Move any face-up card, with all on top")] },
        { key: "pyramid", name: qsTr("Pyramid"), choices: [],
          hints: [qsTr("Pair cards that add up to thirteen")] },
        { key: "tripeaks", name: qsTr("TriPeaks"), choices: [],
          hints: [qsTr("Clear three peaks, one rank up or down")] },
        { key: "golf", name: qsTr("Golf"), choices: [],
          hints: [qsTr("Clear seven columns, one rank up or down")] }
    ]

    function choiceOf(key) {
        if (key === "klondike")
            return page.game.klondikeDraw
        if (key === "spider")
            return page.game.spiderSuits
        return 0
    }

    function choose(key, value) {
        if (key === "klondike")
            page.game.klondikeDraw = value
        else if (key === "spider")
            page.game.spiderSuits = value
    }

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
        contentHeight: content.height + Theme.safeTop + 24 + 96
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Column {
            id: content
            x: 24
            y: Theme.safeTop + 24
            width: parent.width - 48
            spacing: 28

            Column {
                width: parent.width
                spacing: 6
                CalmText {
                    text: qsTr("Solitaire")
                    font.pixelSize: 40
                    font.weight: Font.ExtraBold
                    Accessible.role: Accessible.Heading
                }
                CalmText {
                    width: parent.width
                    text: qsTr("Seven classic games, endless deals. No ads, ever.")
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
                    model: page.games

                    CardButton {
                        id: entry

                        required property var modelData
                        readonly property int choice: page.choiceOf(modelData.key)
                        readonly property int choiceIndex: Math.max(0, modelData.choices.indexOf(choice))

                        width: parent.width
                        text: modelData.name
                        subtitle: modelData.hints[choiceIndex]
                        onClicked: {
                            page.game.startNewGame(page.game.levelFor(modelData.key, choice))
                            page.playRequested()
                        }

                        Row {
                            spacing: 6
                            visible: entry.modelData.choices.length > 0

                            Repeater {
                                model: entry.modelData.choices

                                T.AbstractButton {
                                    id: pick

                                    required property int modelData
                                    readonly property bool chosen: entry.choice === modelData

                                    width: 34
                                    height: 34
                                    text: String(modelData)
                                    Accessible.role: Accessible.RadioButton
                                    Accessible.name: entry.modelData.key === "klondike"
                                                     ? qsTr("Turn %1").arg(modelData) : qsTr("%1 suits").arg(modelData)
                                    Accessible.checked: chosen
                                    onClicked: page.choose(entry.modelData.key, modelData)

                                    contentItem: CalmText {
                                        text: pick.text
                                        horizontalAlignment: Text.AlignHCenter
                                        verticalAlignment: Text.AlignVCenter
                                        font.pixelSize: 15
                                        font.weight: Font.Bold
                                        color: pick.chosen ? Theme.ink : Theme.muted
                                    }
                                    background: Rectangle {
                                        radius: 17
                                        color: pick.chosen ? Theme.selection : Theme.subtle
                                        Behavior on color { ColorAnimation { duration: Theme.quick } }
                                    }
                                }
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
