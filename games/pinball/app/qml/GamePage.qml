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

    // The stage goal on the left; balls and multiplier on the right.
    Item {
        id: scoreboard
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: 24

        Row {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            spacing: 8
            Accessible.role: Accessible.StaticText
            Accessible.name: qsTr("Stage %1: %2 of %3 stars").arg(page.game.stage).arg(page.game.stageStars).arg(page.game.starsNeeded)
                             + (page.game.bankNeeded ? (page.game.bankDone ? qsTr(", bank cleared") : qsTr(", clear the bank")) : "")

            CalmText {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Stage %1").arg(page.game.stage)
                font.weight: Font.Bold
            }
            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2
                Repeater {
                    model: page.game.starsNeeded
                    CalmText {
                        required property int index
                        text: "★"
                        font.pixelSize: 15
                        color: index < page.game.stageStars ? Theme.entry : Theme.trackOff
                        Behavior on color { ColorAnimation { duration: Theme.gentle } }
                    }
                }
            }
            // The bank, when this stage asks for it: three small bars.
            Row {
                anchors.verticalCenter: parent.verticalCenter
                visible: page.game.bankNeeded
                spacing: 2
                Repeater {
                    model: 3
                    Rectangle {
                        width: 6
                        height: 3
                        radius: 1.5
                        color: page.game.bankDone ? Theme.entry : Theme.trackOff
                        Behavior on color { ColorAnimation { duration: Theme.gentle } }
                    }
                }
            }
        }

        Row {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            spacing: 10

            Row {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 5
                visible: !page.game.endless
                Accessible.name: qsTr("%n ball(s) left", "", page.game.ballsLeft)
                Repeater {
                    model: page.game.ballsLeft
                    Rectangle {
                        width: 9
                        height: 9
                        radius: 4.5
                        color: Theme.ink
                    }
                }
            }
            CalmText {
                anchors.verticalCenter: parent.verticalCenter
                text: "×" + page.game.multiplier
                font.weight: Font.Bold
                color: page.game.multiplier > 1 ? Theme.accent : Theme.muted
                Behavior on color { ColorAnimation { duration: Theme.gentle } }
            }
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
                                  : qsTr("Catch the stars to finish the stage")
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
            acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Stylus
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
            acceptedDevices: PointerDevice.TouchScreen | PointerDevice.Stylus
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

    // With a mouse, the left button works the left flipper and the right
    // button the right one (holding it launches a waiting ball).
    MouseArea {
        anchors.top: scoreboard.top
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        acceptedButtons: Qt.LeftButton | Qt.RightButton
        property bool launching: false
        onPressed: (mouse) => {
            // Touches are handled by the screen halves above, not here.
            if (mouse.source !== Qt.MouseEventNotSynthesized) {
                mouse.accepted = false
                return
            }
            if (mouse.button === Qt.LeftButton) {
                page.game.setFlipper(0, true)
            } else {
                launching = page.game.ballReady
                if (launching)
                    page.game.setPlunger(true)
                else
                    page.game.setFlipper(1, true)
            }
        }
        onReleased: (mouse) => {
            if (mouse.button === Qt.LeftButton) {
                page.game.setFlipper(0, false)
            } else if (launching) {
                page.game.setPlunger(false)
                launching = false
            } else {
                page.game.setFlipper(1, false)
            }
        }
        onCanceled: {
            page.game.setFlipper(0, false)
            page.game.setFlipper(1, false)
            if (launching)
                page.game.setPlunger(false)
            launching = false
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
                text: (page.game.score >= page.game.best && page.game.score > 0
                       ? qsTr("%1 · a new best").arg(page.formatScore(page.game.score))
                       : qsTr("%1 · best %2").arg(page.formatScore(page.game.score)).arg(page.formatScore(page.game.best)))
                      + "\n" + qsTr("You reached stage %1").arg(page.game.stage)
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
