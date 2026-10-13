import QtQuick
import QtQuick.Templates as T
import Calm

// The shared statistics screen: one card per level with games played, wins,
// win rate, wins without hints, times and streaks, plus a guarded reset.
//
//   StatsPage { gameId: "sudoku"; levels: ["Easy", "Medium", "Hard"] }
CalmPage {
    id: page

    property string gameId
    // Level ids, in the order the game lists them (the same strings it passes
    // to startNewGame and that Stats records under).
    property var levels: []

    // Bumped after a reset so every card reads Stats again.
    property int revision: 0
    property bool confirmingReset: false

    readonly property bool anyPlayed: {
        page.revision
        for (let i = 0; i < page.levels.length; ++i)
            if (Stats.forLevel(page.gameId, page.levels[i]).played > 0)
                return true
        return false
    }

    function formatTime(seconds) {
        if (seconds <= 0)
            return "–"
        const h = Math.floor(seconds / 3600)
        const m = Math.floor(seconds / 60) % 60
        const s = seconds % 60
        const ss = (s < 10 ? "0" : "") + s
        return h > 0 ? h + ":" + (m < 10 ? "0" : "") + m + ":" + ss : m + ":" + ss
    }

    title: qsTr("Statistics")

    // A small rounded text button for the reset confirmation.
    component PillButton: T.AbstractButton {
        id: pill
        property color fill: Theme.subtle
        property color ink: Theme.ink
        implicitWidth: label.implicitWidth + 40
        implicitHeight: Theme.touch
        Accessible.name: text
        contentItem: CalmText {
            id: label
            text: pill.text
            color: pill.ink
            font.weight: Font.Bold
            horizontalAlignment: Text.AlignHCenter
            verticalAlignment: Text.AlignVCenter
        }
        background: Rectangle {
            radius: Theme.radiusSmall
            color: pill.fill
            opacity: pill.down ? 0.8 : 1
            Behavior on opacity { NumberAnimation { duration: Theme.quick } }
        }
    }

    // One figure in a level card: a value over a short label.
    component Figure: Column {
        property string value
        property string label
        spacing: 2
        Accessible.role: Accessible.StaticText
        Accessible.name: label + ": " + value
        CalmText {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: parent.value
            font.pixelSize: 20
            font.weight: Font.ExtraBold
            Accessible.ignored: true
        }
        CalmText {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            text: parent.label
            font.pixelSize: 12
            font.weight: Font.DemiBold
            color: Theme.muted
            wrapMode: Text.WordWrap
            Accessible.ignored: true
        }
    }

    Flickable {
        anchors.fill: parent
        contentHeight: column.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Column {
            id: column
            width: parent.width
            spacing: 16

            Repeater {
                model: page.levels

                Rectangle {
                    id: card

                    required property var modelData
                    readonly property var stats: {
                        page.revision
                        return Stats.forLevel(page.gameId, card.modelData)
                    }
                    readonly property bool empty: stats.played === 0

                    width: column.width
                    height: cardBody.implicitHeight + 32
                    radius: Theme.radius
                    color: Theme.surface

                    Column {
                        id: cardBody
                        x: 16
                        y: 16
                        width: parent.width - 32
                        spacing: 14

                        Item {
                            width: parent.width
                            height: levelName.implicitHeight

                            CalmText {
                                id: levelName
                                anchors.left: parent.left
                                text: card.modelData
                                font.pixelSize: 18
                                font.weight: Font.Bold
                                Accessible.role: Accessible.Heading
                            }
                            CalmText {
                                anchors.right: parent.right
                                anchors.baseline: levelName.baseline
                                visible: !card.empty
                                text: card.stats.played === 1 ? qsTr("1 game") : qsTr("%1 games").arg(card.stats.played)
                                font.pixelSize: 13
                                color: Theme.muted
                            }
                        }

                        CalmText {
                            width: parent.width
                            visible: card.empty
                            text: qsTr("No games yet. Your results will gather here as you play.")
                            color: Theme.muted
                            wrapMode: Text.WordWrap
                            bottomPadding: 2
                        }

                        // Win rate as a calm bar under the heading.
                        Rectangle {
                            visible: !card.empty
                            width: parent.width
                            height: 6
                            radius: 3
                            color: Theme.subtle
                            Rectangle {
                                width: Math.max(height, parent.width * card.stats.winRate)
                                height: parent.height
                                radius: parent.radius
                                color: Theme.accent
                                visible: card.stats.winRate > 0
                            }
                        }

                        Grid {
                            id: figures
                            visible: !card.empty
                            width: parent.width
                            columns: 4
                            rowSpacing: 16
                            readonly property real cell: width / columns

                            Figure { width: figures.cell; value: card.stats.played; label: qsTr("Played") }
                            Figure { width: figures.cell; value: card.stats.won; label: qsTr("Won") }
                            Figure { width: figures.cell; value: Math.round(card.stats.winRate * 100) + "%"; label: qsTr("Win rate") }
                            Figure { width: figures.cell; value: card.stats.wonNoHints; label: qsTr("No hints") }
                            Figure { width: figures.cell; value: page.formatTime(card.stats.bestSeconds); label: qsTr("Best time") }
                            Figure { width: figures.cell; value: page.formatTime(card.stats.averageSeconds); label: qsTr("Average") }
                            Figure { width: figures.cell; value: card.stats.streak; label: qsTr("Streak") }
                            Figure { width: figures.cell; value: card.stats.bestStreak; label: qsTr("Best streak") }
                        }
                    }
                }
            }

            Item { width: 1; height: 4 }

            // Reset, with an in-page confirmation step.
            Item {
                width: parent.width
                height: page.confirmingReset ? confirm.height : resetButton.height

                PillButton {
                    id: resetButton
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: !page.confirmingReset
                    enabled: page.anyPlayed
                    opacity: enabled ? 1 : 0.4
                    text: qsTr("Reset statistics")
                    fill: "transparent"
                    ink: Theme.danger
                    onClicked: page.confirmingReset = true
                }

                Rectangle {
                    id: confirm
                    visible: page.confirmingReset
                    width: parent.width
                    height: confirmBody.implicitHeight + 32
                    radius: Theme.radius
                    color: Theme.surface

                    Column {
                        id: confirmBody
                        x: 16
                        y: 16
                        width: parent.width - 32
                        spacing: 14

                        CalmText {
                            width: parent.width
                            horizontalAlignment: Text.AlignHCenter
                            text: qsTr("Clear all statistics? This can't be undone.")
                            wrapMode: Text.WordWrap
                            font.weight: Font.DemiBold
                        }
                        Row {
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 12
                            PillButton {
                                text: qsTr("Keep them")
                                onClicked: page.confirmingReset = false
                            }
                            PillButton {
                                text: qsTr("Reset")
                                fill: Theme.danger
                                ink: Theme.knob
                                onClicked: {
                                    Stats.reset(page.gameId)
                                    page.confirmingReset = false
                                    page.revision++
                                }
                            }
                        }
                    }
                }
            }

            CalmText {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Statistics stay on this device.")
                font.pixelSize: 13
                color: Theme.muted
                bottomPadding: 8
            }
        }
    }
}
