import QtQuick
import QtQuick.Templates as T
import Calm

// A row of mutually exclusive options. `model` is a list of labels.
Rectangle {
    id: root

    property var model: []
    property int currentIndex: 0
    signal activated(int index)

    implicitHeight: 52
    radius: Theme.radius
    color: Theme.surface

    Row {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        Repeater {
            model: root.model

            T.AbstractButton {
                id: option
                required property int index
                required property string modelData

                width: (parent.width - (root.model.length - 1) * 4) / root.model.length
                height: parent.height
                text: modelData
                checked: index === root.currentIndex
                Accessible.role: Accessible.RadioButton
                Accessible.name: text
                Accessible.checked: checked
                onClicked: root.activated(index)

                contentItem: CalmText {
                    text: option.text
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    font.pixelSize: 15
                    font.weight: Font.Bold
                    color: option.checked ? Theme.ink : Theme.muted
                }

                background: Rectangle {
                    radius: Theme.radiusSmall
                    color: option.checked ? Theme.selection : "transparent"
                    Behavior on color { ColorAnimation { duration: Theme.quick } }
                }
            }
        }
    }
}
