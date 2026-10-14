import QtQuick
import QtQuick.Templates as T
import Calm
import Sudoku

SettingsPage {
    id: page

    required property SudokuGame game

    // A labelled slider row, styled like the framework's Volume slider.
    component SliderRow: Item {
        id: row
        property alias text: label.text
        property alias from: slider.from
        property alias to: slider.to
        property alias value: slider.value
        signal moved(real value)

        width: parent.width
        height: 52
        CalmText {
            id: label
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            width: 72
            font.weight: Font.DemiBold
        }
        T.Slider {
            id: slider
            anchors.left: label.right
            anchors.right: parent.right
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            anchors.verticalCenter: parent.verticalCenter
            height: 44
            Accessible.name: label.text
            onMoved: row.moved(value)

            background: Rectangle {
                x: slider.leftPadding
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: slider.availableWidth
                height: 4
                radius: 2
                color: Theme.trackOff
            }
            handle: Rectangle {
                x: slider.leftPadding + slider.visualPosition * (slider.availableWidth - width)
                y: slider.topPadding + slider.availableHeight / 2 - height / 2
                width: 24
                height: 24
                radius: 12
                color: Theme.accent
            }
        }
    }

    SettingsGroup {
        width: parent.width
        title: qsTr("Play")

        ToggleRow {
            text: qsTr("Highlight row, column and box")
            checked: page.game.highlightPeers
            onToggled: page.game.highlightPeers = checked
        }
        ToggleRow {
            text: qsTr("Highlight matching numbers")
            checked: page.game.highlightMatching
            onToggled: page.game.highlightMatching = checked
        }
        Item {
            width: parent.width
            height: mistakesControl.y + mistakesControl.height + 6 + mistakesHelp.height + 12
            CalmText {
                id: mistakesLabel
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.verticalCenter: mistakesControl.verticalCenter
                text: qsTr("Mistakes")
                font.weight: Font.DemiBold
            }
            SegmentedControl {
                id: mistakesControl
                anchors.left: mistakesLabel.right
                anchors.right: parent.right
                anchors.leftMargin: 16
                anchors.rightMargin: 8
                y: 8
                height: 44
                color: Theme.ground
                model: [qsTr("Off"), qsTr("Clashes"), qsTr("Answer")]
                currentIndex: page.game.mistakeMode
                Accessible.name: qsTr("Show mistakes")
                onActivated: (index) => page.game.mistakeMode = index
            }
            CalmText {
                id: mistakesHelp
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                anchors.top: mistakesControl.bottom
                anchors.topMargin: 6
                wrapMode: Text.WordWrap
                font.pixelSize: 13
                color: Theme.muted
                text: [qsTr("Mistakes aren't marked."),
                       qsTr("Marks a number that repeats in its row, column or box, or breaks a variant's rule."),
                       qsTr("Marks any number that differs from the answer.")][page.game.mistakeMode]
            }
        }
        ToggleRow {
            text: qsTr("Show timer")
            checked: page.game.showTimer
            onToggled: page.game.showTimer = checked
        }
        ToggleRow {
            text: qsTr("Auto-remove notes")
            checked: page.game.autoRemoveNotes
            onToggled: page.game.autoRemoveNotes = checked
        }
    }

    SettingsGroup {
        width: parent.width
        title: qsTr("Number pad")

        // A live miniature of the pad's width and position on screen.
        Item {
            width: parent.width
            height: 56
            Rectangle {
                id: track
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.margins: 16
                anchors.verticalCenter: parent.verticalCenter
                height: 32
                radius: Theme.radiusSmall
                color: Theme.ground
                Rectangle {
                    width: track.width * page.game.padWidth
                    x: (track.width - width) * (1 + page.game.padOffset) / 2
                    height: parent.height
                    radius: Theme.radiusSmall
                    color: Theme.selection
                    Behavior on x { NumberAnimation { duration: Theme.quick } }
                    Behavior on width { NumberAnimation { duration: Theme.quick } }
                    Row {
                        anchors.centerIn: parent
                        spacing: Math.max(2, parent.width / 14)
                        Repeater {
                            model: 3
                            Rectangle { width: 6; height: 6; radius: 3; color: Theme.accent }
                        }
                    }
                }
            }
        }
        SliderRow {
            text: qsTr("Width")
            from: 0.5 // SudokuGame::MinPadWidth
            to: 1
            value: page.game.padWidth
            onMoved: (value) => page.game.padWidth = value
        }
        SliderRow {
            text: qsTr("Position")
            from: -1
            to: 1
            value: page.game.padOffset
            // Snap to the middle so centring is easy to find again.
            onMoved: (value) => page.game.padOffset = Math.abs(value) < 0.08 ? 0 : value
        }
    }
}
