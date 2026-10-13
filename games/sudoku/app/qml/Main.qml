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
    }

    // Development aids: --play <Level> opens a new game, --settings the settings.
    Component.onCompleted: {
        const args = Qt.application.arguments
        const play = args.indexOf("--play")
        if (play >= 0 && play + 1 < args.length) {
            sudoku.startNewGame(args[play + 1])
            window.push(gamePage)
        } else if (args.indexOf("--settings") >= 0) {
            window.push(settingsPage)
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
}
