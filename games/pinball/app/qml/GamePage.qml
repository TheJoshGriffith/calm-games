import QtQuick
import Calm
import Pinball

CalmPage {
    id: page

    required property PinballGame game
    signal settingsRequested()

    function formatScore(value) { return Number(value).toLocaleString(Qt.locale(), "f", 0) }

    title: game.level
    subtitle: formatScore(game.score)
    focus: true

    // Physics runs only while this page is showing and the app is in front.
    Binding {
        target: page.game
        property: "onTable"
        value: page.visible && Qt.application.state === Qt.ApplicationActive
    }

    actions: IconButton {
        iconPath: Icons.settings
        text: qsTr("Settings")
        onClicked: page.settingsRequested()
    }

    // Balls, multiplier and lane lights.
    Item {
        id: scoreboard
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 24

        Row {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6
            visible: !page.game.endless
            Accessible.name: qsTr("%n ball(s) left", "", page.game.ballsLeft)
            Repeater {
                model: 3
                Rectangle {
                    required property int index
                    width: 10
                    height: 10
                    radius: 5
                    color: index < page.game.ballsLeft ? Theme.ink : Theme.line
                    Behavior on color { ColorAnimation { duration: Theme.gentle } }
                }
            }
        }

        CalmText {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            visible: page.game.endless
            text: qsTr("Best %1").arg(page.formatScore(Math.max(page.game.best, page.game.score)))
            color: Theme.muted
            font.pixelSize: 14
        }

        CalmText {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: "×" + page.game.multiplier
            font.weight: Font.Bold
            color: page.game.multiplier > 1 ? Theme.accent : Theme.muted
            Behavior on color { ColorAnimation { duration: Theme.gentle } }
        }
    }

    PinballTable {
        id: table
        anchors.top: scoreboard.bottom
        anchors.topMargin: 8
        anchors.bottom: hint.top
        anchors.bottomMargin: 8
        anchors.left: parent.left
        anchors.right: parent.right
        game: page.game
    }

    CalmText {
        id: hint
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        horizontalAlignment: Text.AlignHCenter
        text: page.game.ballReady ? qsTr("Hold the right side, then let go to launch")
                                  : qsTr("Touch left or right to flip")
        color: Theme.muted
        font.pixelSize: 14
    }

    // Each half of the screen is a flipper button; with a ball waiting, the
    // right half draws back the plunger instead.
    Item {
        anchors.top: scoreboard.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        width: parent.width / 2
        PointHandler {
            id: leftTouch
            onActiveChanged: page.game.setFlipper(0, active)
        }
    }
    Item {
        anchors.top: scoreboard.top
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        width: parent.width / 2
        PointHandler {
            id: rightTouch
            property bool launching: false
            onActiveChanged: {
                if (active) {
                    launching = page.game.ballReady
                    if (launching)
                        page.game.setPlunger(true)
                    else
                        page.game.setFlipper(1, true)
                } else if (launching) {
                    page.game.setPlunger(false)
                    launching = false
                } else {
                    page.game.setFlipper(1, false)
                }
            }
        }
    }

    Keys.onPressed: (event) => {
        if (event.isAutoRepeat)
            return
        if (event.key === Qt.Key_Left || event.key === Qt.Key_Z) {
            page.game.setFlipper(0, true)
            event.accepted = true
        } else if (event.key === Qt.Key_Right || event.key === Qt.Key_Slash || event.key === Qt.Key_M) {
            page.game.setFlipper(1, true)
            event.accepted = true
        } else if (event.key === Qt.Key_Space || event.key === Qt.Key_Down) {
            page.game.setPlunger(true)
            event.accepted = true
        }
    }
    Keys.onReleased: (event) => {
        if (event.isAutoRepeat)
            return
        if (event.key === Qt.Key_Left || event.key === Qt.Key_Z) {
            page.game.setFlipper(0, false)
            event.accepted = true
        } else if (event.key === Qt.Key_Right || event.key === Qt.Key_Slash || event.key === Qt.Key_M) {
            page.game.setFlipper(1, false)
            event.accepted = true
        } else if (event.key === Qt.Key_Space || event.key === Qt.Key_Down) {
            page.game.setPlunger(false)
            event.accepted = true
        }
    }

    // Game over (Classic): a gentle card over the table.
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
                text: qsTr("Well played")
                font.pixelSize: 34
                font.weight: Font.ExtraBold
            }
            CalmText {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: page.game.score >= page.game.best && page.game.score > 0
                      ? qsTr("%1 · a new best").arg(page.formatScore(page.game.score))
                      : qsTr("%1 · best %2").arg(page.formatScore(page.game.score)).arg(page.formatScore(page.game.best))
                color: Theme.muted
            }
            Item { width: 1; height: 12 }
            CardButton {
                width: parent.width
                text: qsTr("Play again")
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
