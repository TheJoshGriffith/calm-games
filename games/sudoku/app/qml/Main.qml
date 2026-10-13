import QtQuick
import Calm
import Sudoku

AppWindow {
    id: window

    title: qsTr("Sudoku")
    session: sudoku

    SudokuGame {
        id: sudoku
    }

    initialItem: HomePage {
        game: sudoku
        onPlayRequested: window.push(gamePage)
        onSettingsRequested: window.push(settingsPage)
        onStatsRequested: window.push(statsPage)
    }

    // Level ids as Stats records them (and as HomePage starts them).
    readonly property var levels: ["Easy", "Medium", "Hard", "Expert"]

    // Development aids: --play <Level> opens a new game, --settings the
    // settings, --stats the statistics.
    Component.onCompleted: {
        const args = Qt.application.arguments
        const play = args.indexOf("--play")
        if (play >= 0 && play + 1 < args.length) {
            sudoku.startNewGame(args[play + 1])
            window.push(gamePage)
        } else if (args.indexOf("--settings") >= 0) {
            window.push(settingsPage)
        } else if (args.indexOf("--stats") >= 0) {
            window.push(statsPage)
        }
    }

    Component {
        id: gamePage
        GamePage {
            game: sudoku
            onBackRequested: window.pop()
            onSettingsRequested: window.push(settingsPage)
        }
    }

    Component {
        id: settingsPage
        SudokuSettingsPage {
            game: sudoku
            onBackRequested: window.pop()
        }
    }

    Component {
        id: statsPage
        StatsPage {
            gameId: sudoku.gameId
            levels: window.levels
            onBackRequested: window.pop()
        }
    }
}
