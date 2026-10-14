import QtQuick
import QtQuick.Templates as T
import Calm
import Sudoku

CalmPage {
    id: page

    required property SudokuGame game
    signal settingsRequested()
    // A short note for the window to show ("Code copied").
    signal noteRequested(string message)

    objectName: "gamePage"

    function openShareSheet() { shareSheet.open() }

    // Shares the starting board, or the board as it is now, through the
    // system share sheet (or the clipboard, where there is none).
    function share(withProgress) {
        const url = page.game.shareUrl(withProgress)
        if (url.length === 0)
            return
        const text = withProgress ? qsTr("Here's my %1 Sudoku so far: %2")
                                  : qsTr("Try this %1 Sudoku: %2")
        // iPad points its share popover at the header's share button.
        const p = shareButton.mapToItem(null, 0, 0)
        shareSheet.close()
        Sharing.share(text.arg(page.game.level).arg(url), Qt.rect(p.x, p.y, shareButton.width, shareButton.height))
    }

    function formatTime(seconds) {
        const m = Math.floor(seconds / 60)
        const s = seconds % 60
        return m + ":" + (s < 10 ? "0" : "") + s
    }

    title: game.level
    subtitle: game.showTimer ? formatTime(game.elapsedSeconds) : ""

    readonly property bool isVariant: game.variant !== "classic"
    function openRulesSheet() { rulesSheet.open() }

    actions: [
        IconButton {
            // A variant's rules, a tap away.
            visible: page.isVariant
            iconPath: "M12 3a9 9 0 1 0 0 18a9 9 0 1 0 0-18M12 11v6M12 7.5v.5"
            text: qsTr("Rules")
            onClicked: rulesSheet.open()
        },
        IconButton {
            id: shareButton
            iconPath: Icons.share
            text: qsTr("Share")
            enabled: page.game.active && !page.game.loading
            onClicked: shareSheet.open()
        },
        IconButton {
            iconPath: Icons.settings
            text: qsTr("Settings")
            onClicked: page.settingsRequested()
        }
    ]

    // Keyboard play (desktop and the web): 1-9 enter a digit (or paint a
    // colour) just like the pad, Backspace, Delete or 0 erase, the arrow keys
    // move the selection and Escape clears it. Shortcuts give way to text
    // fields and stay off while a sheet is open.
    readonly property bool keysEnabled: T.StackView.status === T.StackView.Active
                                        && !shareSheet.opened && !rulesSheet.opened && !page.game.loading

    function moveSelection(dx, dy) {
        const from = page.game.selected
        if (from < 0) {
            page.game.selectOnly(40)
            return
        }
        const row = Math.max(0, Math.min(8, Math.floor(from / 9) + dy))
        const col = Math.max(0, Math.min(8, from % 9 + dx))
        page.game.selectOnly(row * 9 + col)
    }

    Instantiator {
        model: 9
        delegate: Shortcut {
            required property int index
            sequences: [String(index + 1), "Num+" + (index + 1)]
            enabled: page.keysEnabled
            onActivated: page.game.inputMode === SudokuGame.ColourMode ? page.game.paint(index + 1)
                                                                     : page.game.enter(index + 1)
        }
    }
    Shortcut {
        sequences: ["Backspace", "Delete", "0"]
        enabled: page.keysEnabled
        onActivated: page.game.inputMode === SudokuGame.ColourMode ? page.game.paint(0) : page.game.erase()
    }
    Shortcut {
        sequences: [StandardKey.Undo]
        enabled: page.keysEnabled
        onActivated: page.game.undo()
    }
    Shortcut { sequence: "Left"; enabled: page.keysEnabled; onActivated: page.moveSelection(-1, 0) }
    Shortcut { sequence: "Right"; enabled: page.keysEnabled; onActivated: page.moveSelection(1, 0) }
    Shortcut { sequence: "Up"; enabled: page.keysEnabled; onActivated: page.moveSelection(0, -1) }
    Shortcut { sequence: "Down"; enabled: page.keysEnabled; onActivated: page.moveSelection(0, 1) }
    Shortcut {
        sequence: "Escape"
        enabled: page.keysEnabled
        onActivated: {
            page.game.clearSelection()
            page.game.focusDigit = 0
        }
    }

    // A tap on the page outside the grid and the controls clears the selection
    // and any focused digit.
    // It sits under the page's own background, which takes no input.
    MouseArea {
        parent: page
        anchors.fill: parent
        z: -1
        onClicked: {
            page.game.clearSelection()
            page.game.focusDigit = 0
        }
    }

    // Phones stack the board over the controls. A window wider than it is
    // tall (a desktop browser, an iPad on its side) puts the controls beside
    // the board instead, and the board never grows past the space it has.
    Item {
        id: area
        anchors.fill: parent
        opacity: page.game.loading ? 0.3 : 1
        Behavior on opacity { NumberAnimation { duration: Theme.gentle } }

        readonly property int gap: 16
        readonly property int minControls: 300
        readonly property bool sideBySide: width >= height + gap + minControls
        readonly property real boardSize: sideBySide
            ? Math.min(height, width - gap - minControls)
            : Math.min(width, Math.max(240, height - gap - controlsArea.height))
        readonly property real controlsWidth: sideBySide
            ? Math.min(420, width - boardSize - gap)
            : Math.min(width, Math.max(boardSize, minControls))
        // The board and controls as one centred group.
        readonly property real groupWidth: sideBySide ? boardSize + gap + controlsWidth
                                                      : Math.max(boardSize, controlsWidth)

        Board {
            x: (area.width - area.groupWidth) / 2
               + (area.sideBySide ? 0 : (area.groupWidth - width) / 2)
            y: area.sideBySide ? (area.height - height) / 2 : 0
            width: area.boardSize
            height: area.boardSize
            game: page.game
        }

        Item {
            id: controlsArea
            x: area.sideBySide ? (area.width - area.groupWidth) / 2 + area.boardSize + area.gap
                               : (area.width - width) / 2
            y: area.sideBySide ? (area.height - height) / 2 : area.boardSize + area.gap
            width: area.controlsWidth
            height: controls.height

            // Gaps between controls belong to the controls: a near miss on
            // the pad shouldn't clear the selection.
            MouseArea {
                anchors.fill: parent
            }

            Column {
                id: controls
                width: parent.width
                spacing: 12

                Row {
                    width: parent.width
                    ActionButton {
                        width: parent.width / 4
                        iconPath: Icons.undo
                        text: qsTr("Undo")
                        enabled: page.game.canUndo
                        onClicked: page.game.undo()
                    }
                    ActionButton {
                        width: parent.width / 4
                        iconPath: Icons.erase
                        text: qsTr("Erase")
                        onClicked: page.game.erase()
                    }
                    ActionButton {
                        width: parent.width / 4
                        // Three cells and a tick: several cells at once.
                        iconPath: "M4 4h6v6H4zM14 4h6v6h-6zM4 14h6v6H4zM14 17.5l2.2 2.2L20.5 14.5"
                        text: qsTr("Multi-select")
                        checkable: true
                        checked: page.game.multiSelect
                        onToggled: page.game.multiSelect = checked
                    }
                    ActionButton {
                        width: parent.width / 4
                        iconPath: Icons.hint
                        text: qsTr("Hint")
                        onClicked: page.game.hint()
                    }
                }

                SegmentedControl {
                    width: parent.width
                    height: 44
                    model: [qsTr("Number"), qsTr("Corner"), qsTr("Centre"), qsTr("Colour")]
                    currentIndex: page.game.inputMode
                    Accessible.name: qsTr("Input mode")
                    onActivated: (index) => page.game.inputMode = index
                }

                // Width and side are set in Settings so the pad suits either hand.
                NumberPad {
                    width: parent.width * page.game.padWidth
                    x: (parent.width - width) * (1 + page.game.padOffset) / 2
                    game: page.game
                }
            }
        }
    }

    CalmText {
        anchors.centerIn: parent
        visible: page.game.loading
        text: qsTr("Preparing a fresh puzzle…")
        color: Theme.muted
    }

    // Solved: a gentle card over the board.
    Rectangle {
        id: solved
        anchors.fill: parent
        color: Theme.ground
        opacity: page.game.complete ? 0.94 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.slow; easing.type: Easing.InOutQuad } }

        TapHandler {} // swallow taps on the board underneath

        Column {
            anchors.centerIn: parent
            width: parent.width
            spacing: 12

            CalmText {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Solved")
                font.pixelSize: 34
                font.weight: Font.ExtraBold
            }
            CalmText {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("%1 in %2").arg(page.game.level).arg(page.formatTime(page.game.elapsedSeconds))
                color: Theme.muted
            }
            CalmText {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: page.game.hintsUsed === 0 ? qsTr("No hints")
                      : page.game.hintsUsed === 1 ? qsTr("1 hint used")
                      : qsTr("%1 hints used").arg(page.game.hintsUsed)
                font.pixelSize: 14
                color: Theme.muted
            }
            Item { width: 1; height: 12 }
            CardButton {
                width: parent.width
                text: qsTr("Another %1 puzzle").arg(page.game.level)
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

    // Share: the puzzle code to read out or copy, and links to the starting
    // board or to the board with the player's progress.
    BottomSheet {
        id: shareSheet
        parent: page
        title: qsTr("Share puzzle")

        CardButton {
            width: parent.width
            visible: page.game.puzzleCode.length > 0
            text: page.game.puzzleCode
            subtitle: qsTr("Puzzle code · tap to copy")
            fill: Theme.surface
            onClicked: {
                Sharing.copy(page.game.puzzleCode)
                page.noteRequested(qsTr("Code copied"))
            }
            Icon { path: Icons.copy; color: Theme.accent }
        }
        CalmText {
            width: parent.width
            visible: page.game.puzzleCode.length === 0
            text: qsTr("This puzzle came from a link, so it has no code. You can still share it as a link.")
            wrapMode: Text.WordWrap
            font.pixelSize: 15
            color: Theme.muted
        }
        CardButton {
            width: parent.width
            text: qsTr("Share this puzzle")
            subtitle: qsTr("A link to the starting board")
            onClicked: page.share(false)
            Icon { path: Icons.share; color: Theme.accent }
        }
        CardButton {
            width: parent.width
            text: qsTr("Share my progress")
            subtitle: qsTr("With your digits, notes and colours")
            onClicked: page.share(true)
            Icon { path: Icons.share; color: Theme.accent }
        }
    }

    // A variant's rules: the classic rule, then what this variant adds.
    BottomSheet {
        id: rulesSheet
        parent: page
        title: Variants.name(page.game.variant)

        CalmText {
            width: parent.width
            text: Variants.base
            wrapMode: Text.WordWrap
            font.pixelSize: 15
            color: Theme.muted
        }
        CalmText {
            width: parent.width
            text: Variants.info[page.game.variant] ? Variants.info[page.game.variant].rules : ""
            wrapMode: Text.WordWrap
            font.pixelSize: 17
            lineHeight: 1.1
            bottomPadding: 8
        }
    }
}
