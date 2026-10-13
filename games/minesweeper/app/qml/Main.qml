import QtQuick
import Calm
import Minesweeper

AppWindow {
    id: window

    title: qsTr("Minesweeper")
    session: minesweeper

    MinesweeperGame {
        id: minesweeper
    }

    initialItem: HomePage {
        game: minesweeper
        onPlayRequested: window.push(gamePage)
        onSettingsRequested: window.push(settingsPage)
    }

    // Development aids: --play <Level> opens a new game (add --tap-centre to
    // make the first move), --settings opens the settings.
    Component.onCompleted: {
        const args = Qt.application.arguments
        const play = args.indexOf("--play")
        if (play >= 0 && play + 1 < args.length) {
            minesweeper.startNewGame(args[play + 1])
            window.push(gamePage)
            if (args.indexOf("--tap-centre") >= 0)
                minesweeper.hint()
        } else if (args.indexOf("--settings") >= 0) {
            window.push(settingsPage)
        }
    }

    Component {
        id: gamePage
        GamePage {
            game: minesweeper
            onBackRequested: window.pop()
            onSettingsRequested: window.push(settingsPage)
        }
    }

    Component {
        id: settingsPage
        MinesweeperSettingsPage {
            game: minesweeper
            onBackRequested: window.pop()
        }
    }
}
