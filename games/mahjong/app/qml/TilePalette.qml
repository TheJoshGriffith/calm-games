import QtQuick
import Calm

// The colours of one tile theme, in light or dark. Themes change the tiles'
// ink and body; the table around them always follows the app's Theme.
QtObject {
    id: palette

    // "jade", "classic", "ink", "blossom" or "clear".
    property string theme: "jade"
    readonly property bool dark: Theme.dark
    // "clear" draws numbers and letters instead of pips, for easy reading.
    readonly property bool plain: theme === "clear"

    function pick(light, night) { return palette.dark ? night : light }

    readonly property color face: theme === "classic" ? pick("#FFFBF0", "#33302A")
                                : theme === "blossom" ? pick("#FFF7F6", "#3A2F33")
                                : theme === "ink" ? pick("#FAF8F4", "#2B2D2E")
                                : pick("#FBFCFA", "#26343A")
    // The tile's thickness, seen along its bottom and right edges.
    readonly property color side: theme === "classic" ? pick("#D8C7A0", "#5A4E38")
                                : theme === "blossom" ? pick("#E7C3C6", "#5A3E48")
                                : theme === "ink" ? pick("#C9C6BF", "#45484A")
                                : pick("#A9CBC5", "#2F5050")
    readonly property color edge: pick("#1A000000", "#40000000")

    readonly property color red: theme === "classic" ? pick("#C0392B", "#F08C7E")
                               : theme === "blossom" ? pick("#C2577A", "#EBA3BC")
                               : theme === "ink" ? pick("#A23B2E", "#E09A8C")
                               : pick("#B4483B", "#E59A8F")
    readonly property color green: theme === "classic" ? pick("#1E7B4F", "#7FCFA5")
                                 : theme === "blossom" ? pick("#6E9B6B", "#A9CFA6")
                                 : theme === "ink" ? pick("#2E3133", "#D9D6CF")
                                 : pick("#3D7D78", "#86C2BA")
    readonly property color blue: theme === "classic" ? pick("#1F4E9C", "#8FB3EA")
                                : theme === "blossom" ? pick("#7D6BB0", "#BDB0E3")
                                : theme === "ink" ? pick("#2E3133", "#D9D6CF")
                                : pick("#2F6690", "#8FC1E3")
    readonly property color ink: theme === "classic" ? pick("#1B1B1B", "#EDE8DC")
                               : theme === "blossom" ? pick("#4A3B45", "#F1E4EA")
                               : theme === "ink" ? pick("#1A1A1A", "#EDEBE6")
                               : pick("#1F2A30", "#E4EBEE")
}
