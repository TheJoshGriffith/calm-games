import QtQuick
import Calm
import Mahjong

AppWindow {
    id: window

    title: qsTr("Mahjong")
    session: mahjong

    MahjongGame {
        id: mahjong
    }

    initialItem: HomePage {
        game: mahjong
        onPlayRequested: window.push(gamePage)
        onSettingsRequested: window.push(settingsPage)
        onStatsRequested: window.push(statsPage)
    }

    // Level ids as Stats records them: the layout names.
    readonly property var levels: mahjong.layouts.map(layout => layout.name)

    // Called from main.cpp for share links (https://mahjong.gwynant.app/s/...).
    function openLink(link) {
        if (!mahjong.openShared(link)) {
            window.showNote(qsTr("That link doesn't hold a deal"))
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

    // Development aids: --play <Layout> opens a new game (add --select to pick
    // a free tile and --hint to show a hint), --settings opens the settings,
    // --stats the statistics.
    Component.onCompleted: {
        const args = Qt.application.arguments
        const play = args.indexOf("--play")
        if (play >= 0 && play + 1 < args.length) {
            mahjong.startNewGame(args[play + 1])
            window.push(gamePage)
            if (args.indexOf("--hint") >= 0)
                mahjong.hint()
            if (args.indexOf("--select") >= 0)
                mahjong.tap(mahjong.free.indexOf(true))
        } else if (args.indexOf("--settings") >= 0) {
            window.push(settingsPage)
        } else if (args.indexOf("--stats") >= 0) {
            window.push(statsPage)
        }
    }

    Component {
        id: gamePage
        GamePage {
            game: mahjong
            onBackRequested: window.pop()
            onSettingsRequested: window.push(settingsPage)
            onNoteRequested: (message) => window.showNote(message)
        }
    }

    Component {
        id: settingsPage
        MahjongSettingsPage {
            game: mahjong
            onBackRequested: window.pop()
        }
    }

    Component {
        id: statsPage
        StatsPage {
            gameId: mahjong.gameId
            levels: window.levels
            onBackRequested: window.pop()
        }
    }
}
