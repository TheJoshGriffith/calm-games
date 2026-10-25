pragma Singleton
import QtQuick

// Stroke icons on a 24x24 grid, drawn by Icon.qml in the current ink colour.
QtObject {
    readonly property string back: "M15 5l-7 7 7 7"
    readonly property string close: "M6 6l12 12M18 6L6 18"
    readonly property string settings: "M4 7h10M18 7h2M4 17h4M12 17h8M16 5a2 2 0 1 0 0 4a2 2 0 1 0 0-4M10 15a2 2 0 1 0 0 4a2 2 0 1 0 0-4"
    readonly property string undo: "M9 14L4 9l5-5M4 9h10.5a5.5 5.5 0 0 1 0 11H11"
    readonly property string redo: "M15 14l5-5-5-5M20 9H9.5a5.5 5.5 0 0 0 0 11H13"
    readonly property string erase: "M20 20H8l-4.3-4.3a1 1 0 0 1 0-1.4l9.6-9.6a1 1 0 0 1 1.4 0l5 5a1 1 0 0 1 0 1.4L13 18M8.5 9.5l6 6"
    readonly property string pencil: "M16.5 3.5a2.1 2.1 0 0 1 3 3L7 19l-4 1 1-4z"
    readonly property string hint: "M9 18h6M10 21h4M12 3a6 6 0 0 0-3.5 10.9c.6.4 1 1.1 1 1.8V16h5v-.3c0-.7.4-1.4 1-1.8A6 6 0 0 0 12 3z"
    readonly property string stats: "M5 20V11M12 20V4M19 20v-6"
    readonly property string pause: "M9 5v14M15 5v14"
    readonly property string play: "M7 4l12 8-12 8z"
    readonly property string share: "M12 3v12M8 7l4-4 4 4M9 10H6a1 1 0 0 0-1 1v8a1 1 0 0 0 1 1h12a1 1 0 0 0 1-1v-8a1 1 0 0 0-1-1h-3"
    readonly property string copy: "M9 9h10v11H9zM5 15V4h10"
    readonly property string external: "M14 4h6v6M20 4l-9 9M18 14v5a1 1 0 0 1-1 1H5a1 1 0 0 1-1-1V7a1 1 0 0 1 1-1h5"
    readonly property string code: "M5 9h14M5 15h14M10 4L8 20M16 4l-2 16"
}
