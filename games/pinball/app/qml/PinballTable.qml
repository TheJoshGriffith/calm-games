import QtQuick
import QtQuick.Shapes
import Calm
import Pinball

// Draws the table in the theme's palette, scaled to fit, with the ball and
// flippers following the session every frame.
Item {
    id: table

    required property PinballGame game

    readonly property real unit: Math.min(width / game.tableWidth, height / game.tableHeight)

    function pt(p) { return Qt.point(p.x * unit, p.y * unit) }
    function polyline(points) {
        const out = []
        for (let i = 0; i < points.length; ++i)
            out.push(pt(points[i]))
        return out
    }

    Item {
        id: field
        anchors.horizontalCenter: parent.horizontalCenter
        width: table.game.tableWidth * table.unit
        height: table.game.tableHeight * table.unit
        clip: true

        // The felt and its walls.
        Shape {
            anchors.fill: parent
            ShapePath {
                fillColor: Theme.surface
                strokeColor: Theme.line
                strokeWidth: 1.5
                joinStyle: ShapePath.RoundJoin
                PathPolyline { path: table.polyline(table.game.outline.concat([table.game.outline[0]])) }
            }
            ShapePath {
                fillColor: Theme.subtle
                strokeColor: "transparent"
                PathMultiline {
                    paths: table.game.aprons.map(a => table.polyline(a.concat([a[0]])))
                }
            }
            ShapePath {
                fillColor: "transparent"
                strokeColor: Theme.strongLine
                strokeWidth: Math.max(1.5, 0.8 * table.unit)
                capStyle: ShapePath.RoundCap
                joinStyle: ShapePath.RoundJoin
                PathMultiline {
                    paths: table.game.walls.map(w => table.polyline(w))
                }
            }
        }

        // Rollover lanes: rings that fill in as the ball passes.
        Repeater {
            model: table.game.rollovers
            Rectangle {
                required property var modelData
                required property int index
                readonly property bool lit: table.game.lights[index] === true
                x: (modelData.x - modelData.r) * table.unit
                y: (modelData.y - modelData.r) * table.unit
                width: 2 * modelData.r * table.unit
                height: width
                radius: width / 2
                color: lit ? Theme.accent : Theme.peer
                border.color: lit ? Theme.accent : Theme.line
                border.width: Math.max(1, 0.5 * table.unit)
                Behavior on color { ColorAnimation { duration: Theme.gentle } }
            }
        }

        // Slingshots: soft triangles that glow when they kick.
        Repeater {
            model: table.game.slingshots
            Shape {
                id: sling
                required property var modelData
                required property int index
                property real glow: 0
                anchors.fill: parent
                ShapePath {
                    fillColor: Qt.tint(Theme.subtle, Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.6 * sling.glow))
                    strokeColor: Theme.strongLine
                    strokeWidth: Math.max(1.5, 0.8 * table.unit)
                    joinStyle: ShapePath.RoundJoin
                    PathPolyline { path: table.polyline(sling.modelData.concat([sling.modelData[0]])) }
                }
                NumberAnimation on glow {
                    id: slingGlow
                    running: false
                    from: 1; to: 0
                    duration: Theme.slow
                    easing.type: Easing.OutCubic
                }
                Connections {
                    target: table.game
                    function onSlingshotHit(i) { if (i === sling.index) slingGlow.restart() }
                }
            }
        }

        // Bumpers: round pads with a halo that breathes out on each hit.
        Repeater {
            model: table.game.bumpers
            Item {
                id: bumper
                required property var modelData
                required property int index
                property real glow: 0
                x: (modelData.x - modelData.r) * table.unit
                y: (modelData.y - modelData.r) * table.unit
                width: 2 * modelData.r * table.unit
                height: width

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width * (1 + 0.6 * bumper.glow)
                    height: width
                    radius: width / 2
                    color: "transparent"
                    border.color: Theme.accent
                    border.width: Math.max(1, 0.6 * table.unit)
                    opacity: bumper.glow * 0.7
                }
                Rectangle {
                    anchors.fill: parent
                    radius: width / 2
                    color: Qt.tint(Theme.selection, Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.5 * bumper.glow))
                    border.color: Theme.accent
                    border.width: Math.max(1.5, 0.7 * table.unit)
                    scale: 1 + 0.08 * bumper.glow
                }
                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width * 0.42
                    height: width
                    radius: width / 2
                    color: Theme.raised
                    opacity: 0.85
                }
                NumberAnimation on glow {
                    id: bumperGlow
                    running: false
                    from: 1; to: 0
                    duration: Theme.slow
                    easing.type: Easing.OutCubic
                }
                Connections {
                    target: table.game
                    function onBumperHit(i) { if (i === bumper.index) bumperGlow.restart() }
                }
            }
        }

        // The drop-target bank: three bars that sink away when struck.
        Repeater {
            model: 3
            Rectangle {
                required property int index
                readonly property var target: table.game.dropTargets[index]
                readonly property bool down: target ? target.down : false
                x: target ? target.x1 * table.unit : 0
                y: target ? (target.y1 - 1) * table.unit : 0
                width: target ? (target.x2 - target.x1) * table.unit : 0
                height: 2 * table.unit
                radius: height / 2
                color: Theme.entry
                opacity: down ? 0.18 : 1
                scale: down ? 0.85 : 1
                Behavior on opacity { NumberAnimation { duration: Theme.gentle } }
                Behavior on scale { NumberAnimation { duration: Theme.gentle; easing.type: Easing.OutCubic } }
            }
        }

        // Stars: appear somewhere new, breathe gently, fade if left alone.
        Repeater {
            model: 3
            Item {
                id: star
                required property int index
                readonly property var info: table.game.stars[index]
                readonly property bool active: info ? info.active : false
                readonly property real r: info ? info.r * table.unit : 0
                x: info ? info.x * table.unit - r : 0
                y: info ? info.y * table.unit - r : 0
                width: 2 * r
                height: width
                opacity: active ? 1 - 0.75 * info.fade : 0
                scale: active ? 1 : 0.4
                visible: opacity > 0.01
                Behavior on opacity { NumberAnimation { duration: Theme.gentle } }
                Behavior on scale { NumberAnimation { duration: Theme.gentle; easing.type: Easing.OutBack } }

                Rectangle {
                    anchors.centerIn: parent
                    width: parent.width * 1.5
                    height: width
                    radius: width / 2
                    color: Theme.entry
                    opacity: 0.14
                    SequentialAnimation on scale {
                        running: star.active && !Settings.reduceMotion
                        loops: Animation.Infinite
                        NumberAnimation { from: 0.85; to: 1.1; duration: 1400; easing.type: Easing.InOutSine }
                        NumberAnimation { from: 1.1; to: 0.85; duration: 1400; easing.type: Easing.InOutSine }
                    }
                }
                Shape {
                    anchors.fill: parent
                    ShapePath {
                        scale: Qt.size(star.width / 24, star.width / 24)
                        fillColor: Theme.entry
                        strokeColor: Theme.entry
                        strokeWidth: 1
                        joinStyle: ShapePath.RoundJoin
                        PathSvg { path: "M12 2.5l2.8 5.9 6.4.8-4.7 4.4 1.2 6.4L12 16.9l-5.7 3.1 1.2-6.4-4.7-4.4 6.4-.8z" }
                    }
                }
            }
        }

        // A ring that blooms where a star was caught.
        Repeater {
            model: 3
            Rectangle {
                id: bloom
                required property int index
                property real grow: 0
                readonly property var info: table.game.stars[index]
                property real cx: 0
                property real cy: 0
                x: cx - width / 2
                y: cy - height / 2
                width: (6 + 14 * grow) * table.unit
                height: width
                radius: width / 2
                color: "transparent"
                border.color: Theme.entry
                border.width: Math.max(1, 0.6 * table.unit)
                opacity: grow > 0 && grow < 1 ? 1 - grow : 0
                NumberAnimation on grow {
                    id: bloomAnim
                    running: false
                    from: 0; to: 1
                    duration: Theme.slow
                    easing.type: Easing.OutCubic
                }
                Connections {
                    target: table.game
                    function onStarCollected(slot) {
                        if (slot !== bloom.index || !bloom.info)
                            return
                        bloom.cx = bloom.info.x * table.unit
                        bloom.cy = bloom.info.y * table.unit
                        bloomAnim.restart()
                    }
                }
            }
        }

        // While a ball save is running, a soft bar guards the drain.
        Rectangle {
            x: 44 * table.unit
            y: 166 * table.unit
            width: 12 * table.unit
            height: 1.2 * table.unit
            radius: height / 2
            color: Theme.accent
            opacity: table.game.ballSave ? 0.7 : 0
            Behavior on opacity { NumberAnimation { duration: Theme.gentle } }
        }

        // The plunger at the foot of the launch lane, drawn back as it charges.
        Rectangle {
            readonly property real laneWidth: table.game.tableWidth - 4 - table.game.laneLeft
            x: (table.game.laneLeft + 0.6) * table.unit
            y: (170 + 3 * table.game.plungerCharge) * table.unit
            width: (laneWidth - 1.2) * table.unit
            height: 6 * table.unit
            radius: Theme.radiusSmall / 3
            color: table.game.ballReady ? Theme.accent : Theme.subtle
            Behavior on color { ColorAnimation { duration: Theme.gentle } }
        }

        // Flippers rotate about their pivots.
        Repeater {
            model: table.game.flippers
            Rectangle {
                required property var modelData
                required property int index
                x: (modelData.x - modelData.radius) * table.unit
                y: (modelData.y - modelData.radius) * table.unit
                width: (modelData.length + 2 * modelData.radius) * table.unit
                height: 2 * modelData.radius * table.unit
                radius: height / 2
                color: Theme.accent
                antialiasing: true
                transform: Rotation {
                    origin.x: modelData.radius * table.unit
                    origin.y: modelData.radius * table.unit
                    angle: index === 0 ? table.game.leftAngle : table.game.rightAngle
                }
            }
        }

        // The ball, with a soft shadow and a little highlight.
        Rectangle {
            id: shadow
            readonly property real r: table.game.ballRadius * table.unit
            x: table.game.ballX * table.unit - r + 0.6 * table.unit
            y: table.game.ballY * table.unit - r + 0.9 * table.unit
            width: 2 * r
            height: width
            radius: r
            color: Theme.ink
            opacity: 0.12
            visible: table.game.active
        }
        Rectangle {
            id: ball
            readonly property real r: table.game.ballRadius * table.unit
            x: table.game.ballX * table.unit - r
            y: table.game.ballY * table.unit - r
            width: 2 * r
            height: width
            radius: r
            color: Theme.ink
            visible: table.game.active
            Rectangle {
                x: parent.width * 0.22
                y: parent.width * 0.18
                width: parent.width * 0.32
                height: width
                radius: width / 2
                color: Theme.surface
                opacity: 0.45
            }
        }

        // Short messages ("Stage 2", "Ball saved") that float up and fade.
        CalmText {
            id: announcement
            property real t: 1
            anchors.horizontalCenter: parent.horizontalCenter
            y: (112 - 6 * t) * table.unit
            font.pixelSize: Math.max(18, 6 * table.unit)
            font.weight: Font.ExtraBold
            color: Theme.accent
            opacity: t < 0.15 ? t / 0.15 : 1 - (t - 0.15) / 0.85
            visible: t < 1
            NumberAnimation on t {
                id: announceAnim
                running: false
                from: 0; to: 1
                duration: Settings.reduceMotion ? 1600 : 2200
            }
            Connections {
                target: table.game
                function onAnnounce(text) {
                    announcement.text = text
                    announceAnim.restart()
                }
            }
        }
    }
}
