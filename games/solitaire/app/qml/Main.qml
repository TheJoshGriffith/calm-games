import QtQuick
import Calm
import Solitaire

AppWindow {
    id: window

    title: qsTr("Solitaire")
    session: solitaire

    SolitaireGame {
        id: solitaire
    }

    initialItem: HomePage {
        game: solitaire
        onPlayRequested: window.push(gamePage)
        onSettingsRequested: window.push(settingsPage)
    }

    // Development aids: --play <Level> opens a new game, --settings the settings.
    Component.onCompleted: {
        const args = Qt.application.arguments
        const play = args.indexOf("--play")
        if (play >= 0 && play + 1 < args.length) {
            solitaire.startNewGame(args[play + 1])
            window.push(gamePage)
        } else if (args.indexOf("--settings") >= 0) {
            window.push(settingsPage)
        }
    }

    Component {
        id: gamePage
        GamePage {
            game: solitaire
            onBackRequested: window.pop()
            onSettingsRequested: window.push(settingsPage)
        }
    }

    Component {
        id: settingsPage
        SolitaireSettingsPage {
            game: solitaire
            onBackRequested: window.pop()
        }
    }
}
