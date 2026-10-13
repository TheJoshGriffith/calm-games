import QtQuick
import Calm

// A short note that floats near the bottom of the screen and fades away on
// its own ("Link copied"). Fill the window with it and call show(text). It
// never takes input, so whatever is underneath stays usable.
Item {
    id: toast

    property int duration: 2200

    function show(message) {
        label.text = message
        pill.opacity = 1
        timer.restart()
    }

    Rectangle {
        id: pill
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 40
        width: Math.min(toast.width - 2 * Theme.gutter, label.implicitWidth + 40)
        height: 44
        radius: height / 2
        color: Theme.noteFill
        opacity: 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: Theme.quick } }

        Accessible.role: Accessible.AlertMessage
        Accessible.name: label.text

        CalmText {
            id: label
            anchors.centerIn: parent
            width: Math.min(implicitWidth, pill.width - 32)
            horizontalAlignment: Text.AlignHCenter
            elide: Text.ElideRight
            color: Theme.noteInk
            font.pixelSize: 15
            font.weight: Font.DemiBold
        }
    }

    Timer {
        id: timer
        interval: toast.duration
        onTriggered: pill.opacity = 0
    }
}
