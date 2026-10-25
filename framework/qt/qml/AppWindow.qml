import QtQuick
import QtQuick.Templates as T
import Calm

// The top-level window of every Calm game: a page stack with calm
// transitions, the theme background, the wiring from the active
// GameSession's feedback events to sounds and haptics, and short notes
// (showNote(), and "Link copied" whenever Sharing falls back to the clipboard).
T.ApplicationWindow {
    id: window

    property GameSession session
    property alias stack: stack
    property alias initialItem: stack.initialItem

    width: 390
    height: 844
    visible: true
    color: Theme.ground

    function push(item, properties) { return stack.push(item, properties || {}) }
    function pop() { return stack.pop() }
    // A short note over everything, fading away by itself.
    function showNote(message) { toast.show(message) }

    // Development aid: --size <width>x<height> sets the window size, so every
    // game can be checked (and captured for the stores) at any screen size.
    Component.onCompleted: {
        const args = Qt.application.arguments
        const size = args.indexOf("--size")
        if (size >= 0 && size + 1 < args.length) {
            const wh = args[size + 1].split("x")
            if (wh.length === 2) {
                width = parseInt(wh[0])
                height = parseInt(wh[1])
            }
        }
    }

    T.StackView {
        id: stack
        anchors.fill: parent
        focus: true

        Keys.onBackPressed: (event) => {
            if (stack.depth > 1) {
                stack.pop()
                event.accepted = true
            }
        }

        pushEnter: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.gentle; easing.type: Easing.OutCubic }
                NumberAnimation { property: "x"; from: 24; to: 0; duration: Theme.gentle; easing.type: Easing.OutCubic }
            }
        }
        pushExit: Transition {
            NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.gentle }
        }
        popEnter: Transition {
            NumberAnimation { property: "opacity"; from: 0; to: 1; duration: Theme.gentle; easing.type: Easing.OutCubic }
        }
        popExit: Transition {
            ParallelAnimation {
                NumberAnimation { property: "opacity"; from: 1; to: 0; duration: Theme.gentle }
                NumberAnimation { property: "x"; from: 0; to: 24; duration: Theme.gentle; easing.type: Easing.OutCubic }
            }
        }
        replaceEnter: pushEnter
        replaceExit: pushExit
    }

    Toast {
        id: toast
        anchors.fill: parent
        z: 100
    }

    // A share that had no share sheet to go to landed on the clipboard.
    Connections {
        target: Sharing
        function onCopiedToClipboard() { toast.show(qsTr("Link copied")) }
    }

    Binding { target: SoundBank; property: "enabled"; value: Settings.soundEnabled }
    Binding { target: SoundBank; property: "volume"; value: Settings.volume }
    Binding { target: SoundBank; property: "theme"; value: Settings.soundTheme }
    Binding { target: Haptics; property: "enabled"; value: Settings.hapticsEnabled }
    Binding { target: Haptics; property: "strength"; value: Settings.hapticsStrength }

    Connections {
        target: window.session
        function onFeedback(event, step) {
            SoundBank.play(event, step)
            Haptics.play(event)
        }
    }

    // Pause the clock (and autosave) whenever the app leaves the foreground.
    Connections {
        target: Qt.application
        function onStateChanged() {
            if (window.session)
                window.session.paused = Qt.application.state !== Qt.ApplicationActive
        }
    }
}
