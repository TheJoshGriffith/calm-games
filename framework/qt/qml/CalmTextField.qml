import QtQuick
import QtQuick.Templates as T
import Calm

// A single-line text field in the Calm look. Set `error` to outline it in the
// danger colour while its input is invalid.
T.TextField {
    id: control

    property bool error: false

    implicitWidth: 240
    implicitHeight: 52
    leftPadding: 16
    rightPadding: 16
    verticalAlignment: TextInput.AlignVCenter

    font.family: Theme.fontFamily
    font.pixelSize: 18
    font.weight: Font.DemiBold
    color: Theme.ink
    selectionColor: Theme.selection
    selectedTextColor: Theme.ink
    placeholderTextColor: Theme.muted
    Accessible.name: placeholderText

    background: Rectangle {
        radius: Theme.radiusSmall
        color: Theme.raised
        border.width: control.error || control.activeFocus ? 2 : 1
        border.color: control.error ? Theme.danger : control.activeFocus ? Theme.accent : Theme.line
        Behavior on border.color { ColorAnimation { duration: Theme.quick } }

        // Templates draw no placeholder of their own.
        CalmText {
            anchors.fill: parent
            anchors.leftMargin: control.leftPadding
            anchors.rightMargin: control.rightPadding
            verticalAlignment: Text.AlignVCenter
            visible: control.length === 0 && control.preeditText.length === 0
            text: control.placeholderText
            font.pixelSize: control.font.pixelSize
            color: control.placeholderTextColor
            elide: Text.ElideRight
        }
    }
}
