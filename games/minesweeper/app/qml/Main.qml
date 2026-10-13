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
        onStatsRequested: window.push(statsPage)
    }

    // Level ids as Stats records them (and as HomePage starts them).
    readonly property var levels: ["Easy", "Medium", "Hard", "Expert"]

    // Development aids: --play <Level> opens a new game (add --tap-centre to
    // make the first move), --settings opens the settings, --stats the statistics.
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
        } else if (args.indexOf("--stats") >= 0) {
            window.push(statsPage)
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

    Component {
        id: statsPage
        StatsPage {
            gameId: minesweeper.gameId
            levels: window.levels
            onBackRequested: window.pop()
        }
    }
}
