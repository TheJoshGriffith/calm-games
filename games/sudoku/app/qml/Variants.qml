pragma Singleton
import QtQuick

// The variants the app offers, in menu order, with a one-line summary for the
// picker and the rules for the info sheet. `id` matches SudokuGame.variant.
QtObject {
    readonly property string base: qsTr("Fill the grid so every row, column and box holds the digits 1 to 9 once each.")

    readonly property var groups: [
        { title: qsTr("Extra rules"), ids: ["diagonal", "anti-knight", "anti-king", "windoku", "non-consecutive", "jigsaw"] },
        { title: qsTr("Clues on the grid"), ids: ["killer", "thermo", "arrow", "sandwich", "kropki", "xv", "even-odd",
                                                  "little-killer", "whispers", "renban", "palindrome"] }
    ]

    readonly property var info: ({
        "classic": { name: qsTr("Classic"), summary: "", rules: "" },
        "diagonal": {
            name: qsTr("Diagonal"),
            summary: qsTr("Both long diagonals hold 1 to 9"),
            rules: qsTr("Each of the two long diagonals also holds the digits 1 to 9 once each.")
        },
        "anti-knight": {
            name: qsTr("Anti-Knight"),
            summary: qsTr("No repeats a knight's move apart"),
            rules: qsTr("Two cells a chess knight's move apart (two steps one way, one step the other) never hold the same digit.")
        },
        "anti-king": {
            name: qsTr("Anti-King"),
            summary: qsTr("No repeats in touching cells"),
            rules: qsTr("Cells that touch, even only at a corner, never hold the same digit.")
        },
        "windoku": {
            name: qsTr("Windoku"),
            summary: qsTr("Four extra shaded boxes"),
            rules: qsTr("Each of the four shaded 3×3 boxes also holds the digits 1 to 9 once each.")
        },
        "non-consecutive": {
            name: qsTr("Non-Consecutive"),
            summary: qsTr("Neighbours are never one apart"),
            rules: qsTr("Cells that share an edge never hold consecutive digits, such as 4 and 5.")
        },
        "jigsaw": {
            name: qsTr("Jigsaw"),
            summary: qsTr("Irregular regions instead of boxes"),
            rules: qsTr("The boxes are irregular shapes, outlined in bold. Each one still holds the digits 1 to 9 once each.")
        },
        "killer": {
            name: qsTr("Killer"),
            summary: qsTr("Cages that add up"),
            rules: qsTr("The digits in each dashed cage add up to the small number in its corner. Digits never repeat within a cage.")
        },
        "thermo": {
            name: qsTr("Thermo"),
            summary: qsTr("Digits rise from the bulb"),
            rules: qsTr("Along each thermometer, digits increase from the round bulb to the far end.")
        },
        "arrow": {
            name: qsTr("Arrow"),
            summary: qsTr("Arrows add up to their circle"),
            rules: qsTr("The digits along an arrow add up to the digit in its circle. Digits may repeat on an arrow where the usual rules allow.")
        },
        "sandwich": {
            name: qsTr("Sandwich"),
            summary: qsTr("Sums between the 1 and the 9"),
            rules: qsTr("A number beside a row or above a column is the sum of the digits between its 1 and its 9.")
        },
        "kropki": {
            name: qsTr("Kropki"),
            summary: qsTr("White and black dots"),
            rules: qsTr("A white dot joins consecutive digits, such as 3 and 4. A black dot joins two digits where one is double the other, such as 3 and 6. Every dot is shown, so neighbours without a dot are neither. 1 and 2 may have either dot.")
        },
        "xv": {
            name: qsTr("XV"),
            summary: qsTr("X adds to 10, V adds to 5"),
            rules: qsTr("Two cells with an X between them add up to 10, and with a V, to 5. Every X and V is shown, so neighbours without one never add up to 5 or 10.")
        },
        "even-odd": {
            name: qsTr("Even-Odd"),
            summary: qsTr("Shaded squares and circles"),
            rules: qsTr("A shaded square holds an even digit; a shaded circle holds an odd digit.")
        },
        "little-killer": {
            name: qsTr("Little Killer"),
            summary: qsTr("Diagonal sums from outside"),
            rules: qsTr("A number outside the grid is the sum of the digits along the diagonal its arrow points down. Digits may repeat on a diagonal where the usual rules allow.")
        },
        "whispers": {
            name: qsTr("German Whispers"),
            summary: qsTr("Neighbours on green lines differ by 5"),
            rules: qsTr("Neighbouring digits along a green line differ by at least 5.")
        },
        "renban": {
            name: qsTr("Renban"),
            summary: qsTr("Purple lines of consecutive digits"),
            rules: qsTr("The digits on a purple line are a run of consecutive numbers in any order, such as 4, 2, 3. They don't repeat.")
        },
        "palindrome": {
            name: qsTr("Palindrome"),
            summary: qsTr("Blue lines read the same both ways"),
            rules: qsTr("Each blue line reads the same from either end: its first and last digits match, then the second and second-to-last, and so on.")
        }
    })

    function name(id) { return info[id] ? info[id].name : id }
}
