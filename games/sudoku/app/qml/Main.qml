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
        id: homePage
        game: sudoku
        onPlayRequested: window.push(gamePage)
        onSettingsRequested: window.push(settingsPage)
        onStatsRequested: window.push(statsPage)
        onVariantsRequested: window.push(variantsPage)
    }

    // Level ids as Stats records them (and as HomePage starts them). Each
    // variant is its own level, named after it.
    readonly property var levels: ["Easy", "Medium", "Hard", "Expert"]

    // The statistics screen lists the classic levels and every variant
    // played at least once.
    function statsLevels() {
        const out = levels.slice()
        for (const id of sudoku.variantIds()) {
            const level = sudoku.variantLevel(id)
            if (Stats.forLevel(sudoku.gameId, level).played > 0)
                out.push(level)
        }
        return out
    }

    // Opens a share link (main.cpp passes web links and iOS universal links
    // here) and shows the game. Returns false if the link holds no puzzle.
    function openLink(link) {
        if (!sudoku.openShared(link)) {
            window.showNote(qsTr("That link doesn't hold a puzzle"))
            return false
        }
        const page = showGame()
        // Development aid: --share <link> --share-sheet shows its share sheet.
        if (Qt.application.arguments.indexOf("--share-sheet") >= 0)
            page.openShareSheet()
        return true
    }

    // Brings the game screen to the front, from wherever the player is.
    function showGame() {
        const current = window.stack.currentItem
        if (current && current.objectName === "gamePage")
            return current
        if (window.stack.depth > 1)
            window.stack.pop(window.stack.get(0))
        return window.push(gamePage)
    }

    Connections {
        target: sudoku
        function onCodeFailed() {
            window.showNote(qsTr("Sorry, that code didn't make a puzzle"))
            if (window.stack.depth > 1)
                window.stack.pop(window.stack.get(0))
        }
    }

    // Development aids: --play <Level> opens a new game (--variant <id> one of
    // a variant, such as killer or anti-knight; --rules then shows its rules),
    // --variants the variant picker, --settings the settings, --stats the
    // statistics. --dark shows the dark theme for this
    // run only. With --play, --demo-input seeds marks, colours, a clash and a
    // multi-selection for screenshots, --mode <Number|Corner|Centre|Colour>
    // picks the input mode, --focus <digit> focuses a digit with nothing
    // selected, and --share-sheet opens the share sheet. Without --play,
    // --enter-code [code] opens the code sheet on the home screen (and tries
    // the code, if one is given).
    property string themeBeforeDev: ""
    property bool demoPending: false
    property bool demoInput: false
    property int demoFocus: 0
    property bool demoShareSheet: false
    property bool demoRules: false
    property Item demoPage: null

    Component.onCompleted: {
        const args = Qt.application.arguments
        if (args.indexOf("--dark") >= 0) {
            themeBeforeDev = Settings.theme
            Settings.theme = "dark"
        }
        let play = args.indexOf("--play")
        let level = play >= 0 && play + 1 < args.length ? args[play + 1] : ""
        const variant = args.indexOf("--variant")
        if (variant >= 0 && variant + 1 < args.length)
            level = sudoku.variantLevel(args[variant + 1])
        if (level !== "") {
            demoInput = args.indexOf("--demo-input") >= 0
            const focus = args.indexOf("--focus")
            demoFocus = focus >= 0 && focus + 1 < args.length ? parseInt(args[focus + 1]) : 0
            demoShareSheet = args.indexOf("--share-sheet") >= 0
            demoRules = args.indexOf("--rules") >= 0
            demoPending = demoInput || demoFocus > 0 || demoShareSheet || demoRules
            sudoku.startNewGame(level)
            demoPage = window.push(gamePage)
            seedDemoWhenReady()
        } else if (args.indexOf("--enter-code") >= 0) {
            const code = args.indexOf("--enter-code")
            const text = code + 1 < args.length && !args[code + 1].startsWith("--") ? args[code + 1] : ""
            homePage.openCodeSheet(text)
        } else if (args.indexOf("--variants") >= 0) {
            window.push(variantsPage)
        } else if (args.indexOf("--settings") >= 0) {
            window.push(settingsPage)
        } else if (args.indexOf("--stats") >= 0) {
            window.push(statsPage)
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
        if (demoInput)
            seedDemo()
        if (demoFocus > 0) {
            sudoku.clearSelection()
            sudoku.enter(demoFocus)
        }
        if (demoShareSheet && demoPage)
            demoPage.openShareSheet()
        if (demoRules && demoPage)
            demoPage.openRulesSheet()
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
            // Home, even when the game was started from the variant picker.
            onBackRequested: window.stack.pop(window.stack.get(0))
            onSettingsRequested: window.push(settingsPage)
            onNoteRequested: (message) => window.showNote(message)
        }
    }

    Component {
        id: variantsPage
        VariantsPage {
            game: sudoku
            onBackRequested: window.pop()
            onPlayRequested: window.push(gamePage)
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
            levels: window.statsLevels()
            onBackRequested: window.pop()
        }
    }
}
