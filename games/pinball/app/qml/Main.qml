import QtQuick
import Calm
import Pinball

AppWindow {
    id: window

    title: qsTr("Pinball")
    session: pinball

    PinballGame {
        id: pinball
    }

    initialItem: HomePage {
        game: pinball
        onPlayRequested: window.push(gamePage)
        onSettingsRequested: window.push(settingsPage)
    }

    // Development aids: --play <Level> opens a new game (add --launch to send
    // the ball up straight away), --settings the settings.
    Component.onCompleted: {
        const args = Qt.application.arguments
        const play = args.indexOf("--play")
        if (play >= 0 && play + 1 < args.length) {
            pinball.startNewGame(args[play + 1])
            window.push(gamePage)
            if (args.indexOf("--launch") >= 0) {
                pinball.setPlunger(true)
                pinball.setPlunger(false)
            }
        } else if (args.indexOf("--settings") >= 0) {
            window.push(settingsPage)
        }
    }

    Component {
        id: gamePage
        GamePage {
            game: pinball
            onBackRequested: window.pop()
            onSettingsRequested: window.push(settingsPage)
        }
    }

    Component {
        id: settingsPage
        SettingsPage {
            onBackRequested: window.pop()
        }
    }
}
