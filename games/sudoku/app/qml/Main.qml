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
    // --dark shows the dark theme for this run only. With --play,
    // --demo-input seeds marks, colours, a clash and a multi-selection for
    // screenshots, and --mode <Number|Corner|Centre|Colour> picks the input mode.
    property string themeBeforeDev: ""
    property bool demoPending: false

    Component.onCompleted: {
        const args = Qt.application.arguments
        if (args.indexOf("--dark") >= 0) {
            themeBeforeDev = Settings.theme
            Settings.theme = "dark"
        }
        const play = args.indexOf("--play")
        if (play >= 0 && play + 1 < args.length) {
            demoPending = args.indexOf("--demo-input") >= 0
            sudoku.startNewGame(args[play + 1])
            window.push(gamePage)
            seedDemoWhenReady()
        } else if (args.indexOf("--settings") >= 0) {
            window.push(settingsPage)
        }
    }

    Connections {
        target: Qt.application
        function onAboutToQuit() {
            if (window.themeBeforeDev !== "")
                Settings.theme = window.themeBeforeDev
        }
    }

    Connections {
        target: sudoku
        enabled: window.demoPending
        function onLoadingChanged() { window.seedDemoWhenReady() }
        function onActiveChanged() { window.seedDemoWhenReady() }
    }

    function seedDemoWhenReady() {
        if (!demoPending || !sudoku.active || sudoku.loading)
            return
        demoPending = false
        seedDemo()
    }

    function seedDemo() {
        // Two correct player digits.
        sudoku.hint()
        sudoku.selectOnly(-1)
        sudoku.hint()

        const start = sudoku.cells
        const empty = [], givens = []
        for (let i = 0; i < 81; ++i) {
            if (start[i].digit === 0)
                empty.push(i)
            else if (start[i].given)
                givens.push(i)
        }
        const used = []
        // The first unused cell from `list` at or after `fraction` of the way through.
        function take(list, fraction) {
            for (let k = Math.floor(fraction * list.length); k < list.length + Math.floor(fraction * list.length); ++k) {
                const c = list[k % list.length]
                if (used.indexOf(c) < 0) {
                    used.push(c)
                    return c
                }
            }
            return -1
        }
        function select(list) {
            sudoku.selectOnly(list[0])
            for (let k = 1; k < list.length; ++k)
                sudoku.addToSelection(list[k])
        }

        // A clash: copy a given from the same row into an empty cell.
        const clash = take(empty, 0.15)
        const rowGiven = givens.find(g => Math.floor(g / 9) === Math.floor(clash / 9))
        sudoku.inputMode = SudokuGame.NumberMode
        sudoku.selectOnly(clash)
        if (rowGiven !== undefined)
            sudoku.enter(start[rowGiven].digit)

        sudoku.inputMode = SudokuGame.CornerMode
        select([take(empty, 0.4), take(empty, 0.42)])
        sudoku.enter(1); sudoku.enter(4); sudoku.enter(7)
        select([take(empty, 0.55)])
        for (const d of [2, 3, 5, 6, 8]) sudoku.enter(d)

        sudoku.inputMode = SudokuGame.CentreMode
        select([take(empty, 0.65)])
        sudoku.enter(3); sudoku.enter(8)
        select([take(empty, 0.75)])
        for (const d of [1, 2, 4, 5, 6, 9]) sudoku.enter(d)

        sudoku.inputMode = SudokuGame.ColourMode
        const paints = [[6, 0.05, 0.1], [4, 0.3, 0.32], [1, 0.5, 0.5], [3, 0.85, 0.7], [8, 0.95, 0.9], [7, 0.6, 0.62]]
        for (const p of paints) {
            select([take(empty, p[1]), take(givens, p[2])])
            sudoku.paint(p[0])
        }

        // A multi-selection, one of its cells coloured.
        sudoku.multiSelect = true
        select([take(empty, 0.88), take(givens, 0.8), take(empty, 0.92)])
        const args = Qt.application.arguments
        const m = args.indexOf("--mode")
        const modes = ["Number", "Corner", "Centre", "Colour"]
        sudoku.inputMode = m >= 0 && m + 1 < args.length ? Math.max(0, modes.indexOf(args[m + 1])) : SudokuGame.NumberMode
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
