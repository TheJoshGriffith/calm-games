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

                // Sound theme: one chip per theme; picking one plays a sample.
                Column {
                    width: parent.width
                    topPadding: 10
                    bottomPadding: 12
                    spacing: 10
                    enabled: Settings.soundEnabled
                    opacity: enabled ? 1 : 0.4

                    CalmText {
                        x: 16
                        text: qsTr("Sound theme")
                        font.weight: Font.DemiBold
                    }

                    Flow {
                        x: 12
                        width: parent.width - 24
                        spacing: 8

                        Repeater {
                            model: SoundBank.themes

                            T.AbstractButton {
                                id: themeChip
                                required property var modelData

                                implicitWidth: chipLabel.implicitWidth + 32
                                implicitHeight: 44
                                text: modelData.name
                                checked: Settings.soundTheme === modelData.id
                                Accessible.role: Accessible.RadioButton
                                Accessible.name: text
                                Accessible.checked: checked
                                onClicked: {
                                    Settings.soundTheme = modelData.id
                                    SoundBank.preview()
                                }

                                contentItem: CalmText {
                                    id: chipLabel
                                    text: themeChip.text
                                    horizontalAlignment: Text.AlignHCenter
                                    verticalAlignment: Text.AlignVCenter
                                    font.pixelSize: 15
                                    font.weight: Font.Bold
                                    color: themeChip.checked ? Theme.ink : Theme.muted
                                }

                                background: Rectangle {
                                    radius: height / 2
                                    color: themeChip.checked ? Theme.selection : Theme.ground
                                    Behavior on color { ColorAnimation { duration: Theme.quick } }
                                }
                            }
                        }
                    }
                }

                ToggleRow {
                    text: qsTr("Haptics")
                    checked: Settings.hapticsEnabled
                    onToggled: Settings.hapticsEnabled = checked
                }

                Item {
                    width: parent.width
                    height: 60
                    CalmText {
                        id: strengthLabel
                        anchors.left: parent.left
                        anchors.leftMargin: 16
                        anchors.verticalCenter: parent.verticalCenter
                        text: qsTr("Strength")
                        font.weight: Font.DemiBold
                    }
                    SegmentedControl {
                        anchors.left: strengthLabel.right
                        anchors.right: parent.right
                        anchors.leftMargin: 16
                        anchors.rightMargin: 8
                        anchors.verticalCenter: parent.verticalCenter
                        height: 44
                        color: Theme.ground
                        model: [qsTr("Subtle"), qsTr("Gentle"), qsTr("Firm")]
                        currentIndex: Settings.hapticsStrength
                        enabled: Settings.hapticsEnabled
                        opacity: enabled ? 1 : 0.4
                        Accessible.name: qsTr("Haptic strength")
                        onActivated: (index) => {
                            Settings.hapticsStrength = index
                            Haptics.play("tap")
                        }
                    }
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
