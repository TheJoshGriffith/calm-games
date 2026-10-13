pragma Singleton
import QtQuick
import Calm

// Every colour, size and duration in a Calm game comes from here, so games
// never hard-code a look and light/dark switching is automatic.
QtObject {
    // Qt::ColorScheme::Dark == 2 (Qt 6.5+); older Qt reports undefined.
    readonly property bool systemDark: Qt.styleHints.colorScheme === 2
    readonly property bool dark: Settings.theme === "dark" || (Settings.theme === "system" && systemDark)

    readonly property color ground: dark ? "#151D22" : "#F2F4F3"
    readonly property color surface: dark ? "#1C262C" : "#FFFFFF"
    readonly property color raised: dark ? "#1F2A31" : "#FFFFFF"
    readonly property color subtle: dark ? "#26333B" : "#E3E9EB"
    readonly property color ink: dark ? "#E4EBEE" : "#1F2A30"
    readonly property color muted: dark ? "#9AABB3" : "#5E6E76"
    readonly property color line: dark ? "#2E3B42" : "#D5DCDF"
    readonly property color strongLine: dark ? "#7D8F98" : "#6B7B84"
    readonly property color accent: dark ? "#86C2BA" : "#3D7D78"
    readonly property color selection: dark ? "#2F5050" : "#CFE5E1"
    readonly property color match: dark ? "#26403F" : "#DDEBE8"
    readonly property color peer: dark ? "#222F35" : "#EEF3F2"
    readonly property color entry: dark ? "#8FC1E3" : "#2F6690"
    readonly property color danger: dark ? "#E59A8F" : "#B4483B"
    readonly property color trackOff: dark ? "#3A4950" : "#C9D2D6"
    readonly property color knob: dark ? "#151D22" : "#FFFFFF"

    readonly property int radius: 16
    readonly property int radiusSmall: 12
    readonly property int gutter: 16
    readonly property int touch: 44
    // Space kept clear of the status bar and notch on phones.
    readonly property int safeTop: (Qt.platform.os === "ios" || Qt.platform.os === "android") ? 48 : 12

    // Durations collapse to zero when the player asks for reduced motion.
    readonly property int quick: Settings.reduceMotion ? 0 : 180
    readonly property int gentle: Settings.reduceMotion ? 0 : 320
    readonly property int slow: Settings.reduceMotion ? 0 : 700

    // Static Nunito weights (instanced from the OFL variable font); loading all
    // four registers one family whose weights Text picks via font.weight.
    property FontLoader fontRegular: FontLoader { source: "qrc:/qt/qml/Calm/fonts/Nunito-Regular.ttf" }
    property FontLoader fontSemiBold: FontLoader { source: "qrc:/qt/qml/Calm/fonts/Nunito-SemiBold.ttf" }
    property FontLoader fontBold: FontLoader { source: "qrc:/qt/qml/Calm/fonts/Nunito-Bold.ttf" }
    property FontLoader fontExtraBold: FontLoader { source: "qrc:/qt/qml/Calm/fonts/Nunito-ExtraBold.ttf" }
    readonly property string fontFamily: fontRegular.status === FontLoader.Ready ? fontRegular.name : Qt.application.font.family
}
