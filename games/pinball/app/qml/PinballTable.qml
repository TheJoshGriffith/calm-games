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
    }
}
