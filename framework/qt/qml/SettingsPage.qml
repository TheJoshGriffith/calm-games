import QtQuick
import QtQuick.Templates as T
import Calm

// The shared settings screen. Games add their own groups as children; they
// appear between "Sound and touch" and "Accessibility".
CalmPage {
    id: page

    default property alias gameSettings: gameGroups.data

    title: qsTr("Settings")

    Flickable {
        anchors.fill: parent
        contentHeight: column.implicitHeight
        boundsBehavior: Flickable.StopAtBounds
        clip: true

        Column {
            id: column
            width: parent.width
            spacing: 24

            Column {
                width: parent.width
                spacing: 8
                CalmText {
                    x: 4
                    text: qsTr("THEME")
                    font.pixelSize: 13
                    font.weight: Font.Bold
                    font.letterSpacing: 1
                    color: Theme.muted
                }
                SegmentedControl {
                    width: parent.width
                    model: [qsTr("Light"), qsTr("Dark"), qsTr("System")]
                    currentIndex: ["light", "dark", "system"].indexOf(Settings.theme)
                    onActivated: (index) => Settings.theme = ["light", "dark", "system"][index]
                }
            }

            SettingsGroup {
                width: parent.width
                title: qsTr("Sound and touch")

                ToggleRow {
                    text: qsTr("Sound effects")
                    checked: Settings.soundEnabled
                    onToggled: Settings.soundEnabled = checked
                }

                Item {
                    width: parent.width
                    height: 52
                    CalmText {
                        id: volumeLabel
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Volume")
                        font.weight: Font.DemiBold
                    }
                    T.Slider {
                        id: volume
                        anchors.left: volumeLabel.right
                        anchors.right: parent.right
                        anchors.leftMargin: 16
                        anchors.rightMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        height: 44
                        from: 0
                        to: 1
                        value: Settings.volume
                        enabled: Settings.soundEnabled
                        opacity: enabled ? 1 : 0.4
                        Accessible.name: qsTr("Volume")
                        onMoved: Settings.volume = value
                        onPressedChanged: if (!pressed) SoundBank.play("tap")

                        background: Rectangle {
                            x: volume.leftPadding
                            y: volume.topPadding + volume.availableHeight / 2 - height / 2
                            width: volume.availableWidth
                            height: 4
                            radius: 2
                            color: Theme.trackOff
                            Rectangle {
                                width: volume.visualPosition * parent.width
                                height: parent.height
                                radius: 2
                                color: Theme.accent
                            }
                        }
                        handle: Rectangle {
                            x: volume.leftPadding + volume.visualPosition * (volume.availableWidth - width)
                            y: volume.topPadding + volume.availableHeight / 2 - height / 2
                            width: 24
                            height: 24
                            radius: 12
                            color: Theme.accent
                        }
                    }
                }

                ToggleRow {
                    text: qsTr("Haptics")
                    checked: Settings.hapticsEnabled
                    onToggled: Settings.hapticsEnabled = checked
                }
            }

            Column {
                id: gameGroups
                width: parent.width
                visible: children.length > 0
                spacing: 24
            }

            SettingsGroup {
                width: parent.width
                title: qsTr("Accessibility")

                ToggleRow {
                    text: qsTr("Reduce motion")
                    checked: Settings.reduceMotion
                    onToggled: Settings.reduceMotion = checked
                }
            }

            CalmText {
                width: parent.width
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Free and open source. No ads, no tracking.")
                font.pixelSize: 13
                color: Theme.muted
                bottomPadding: 8
            }
        }
    }
}
