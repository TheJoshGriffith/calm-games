pragma Singleton
import QtQuick
import Calm

// The nine cell colours for Colour mode. Soft pastels on the light theme and
// muted deep tones on the dark one, chosen so given (ink) and player (entry)
// digits keep comfortable contrast on every swatch in both themes.
QtObject {
    readonly property var light: [
        "#F3CBC6", // 1 rose
        "#F6D9BA", // 2 apricot
        "#F1E5A9", // 3 butter
        "#D2E7C6", // 4 sage
        "#C2E4DD", // 5 mint
        "#C9DDF0", // 6 sky
        "#D9D1F0", // 7 lavender
        "#EED0E4", // 8 orchid
        "#D8DDE0"  // 9 stone
    ]
    readonly property var dark: [
        "#5C3A3C", // 1 rose
        "#5D4630", // 2 apricot
        "#544D2B", // 3 butter
        "#3A5034", // 4 sage
        "#26514C", // 5 mint
        "#2E4865", // 6 sky
        "#463F66", // 7 lavender
        "#583A56", // 8 orchid
        "#434D53"  // 9 stone
    ]
    readonly property var names: [
        qsTr("Rose"), qsTr("Apricot"), qsTr("Butter"), qsTr("Sage"), qsTr("Mint"),
        qsTr("Sky"), qsTr("Lavender"), qsTr("Orchid"), qsTr("Stone")
    ]

    // Colour `n` (1-9) for the current theme; transparent for 0.
    function colour(n) {
        if (n < 1 || n > 9)
            return "transparent"
        return Theme.dark ? dark[n - 1] : light[n - 1]
    }

    // Variant clue lines, soft enough to sit under the digits: thermometers
    // grey, German whispers green, renban purple, palindromes blue.
    readonly property color thermo: Theme.dark ? "#3C4B53" : "#CDD5D9"
    readonly property color whisper: Theme.dark ? "#3E6A3B" : "#A8D49A"
    readonly property color renban: Theme.dark ? "#56477C" : "#CDBAEE"
    readonly property color palindrome: Theme.dark ? "#365879" : "#A9C7E5"
}
