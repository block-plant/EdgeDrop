import QtQuick
import QtQuick.Controls
import EdgeDrop

Item {
    id: root
    width: parent ? parent.width : 284
    implicitHeight: contentCol.height

    signal closed()

    Column {
        id: contentCol
        anchors.fill: parent
        spacing: 14

        // Header Title
        Item {
            width: parent.width
            height: 20

            Text {
                text: "Settings"
                font.pixelSize: 15
                font.weight: Font.DemiBold
                color: Theme.textPrimary
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
            }
        }

        // ── 1. Save Location ──────────────────────────────────────────────────
        Column {
            width: parent.width
            spacing: 6

            Text {
                text: "Save files to"
                font.pixelSize: 11
                font.weight: Font.Medium
                color: Theme.textSecondary
            }

            Rectangle {
                width: parent.width
                height: 38
                radius: 10
                color: Theme.surfaceElevated
                border.color: Theme.borderHairline
                border.width: 1

                Row {
                    anchors.fill: parent
                    anchors.leftMargin: 12
                    anchors.rightMargin: 4
                    spacing: 8

                    Text {
                        width: parent.width - 76
                        text: {
                            var p = settingsManager.outputDirectory;
                            if (!p || p.length === 0) return "~/Downloads";
                            return p.replace(/^\/Users\/[^\/]+/, "~");
                        }
                        font.pixelSize: 12
                        font.family: Theme.monoFont
                        color: Theme.textPrimary
                        elide: Text.ElideMiddle
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Rectangle {
                        width: 64
                        height: 30
                        radius: 7
                        anchors.verticalCenter: parent.verticalCenter
                        color: changeHover.hovered ? Theme.accentHover : Theme.accent

                        scale: changeMouse.pressed ? 0.98 : 1.0
                        Behavior on scale { NumberAnimation { duration: 100 } }
                        Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                        HoverHandler {
                            id: changeHover
                            blocking: false
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "Change"
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            color: Theme.textOnAccent
                        }

                        MouseArea {
                            id: changeMouse
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: appController.chooseOutputFolder()
                        }
                    }
                }
            }
        }

        // ── 2. Auto-Hide Delay ────────────────────────────────────────────────
        Column {
            width: parent.width
            spacing: 6

            Text {
                text: "Auto-hide after"
                font.pixelSize: 11
                font.weight: Font.Medium
                color: Theme.textSecondary
            }

            Rectangle {
                width: parent.width
                height: 36
                radius: 10
                color: Theme.surfaceElevated
                border.color: Theme.borderHairline
                border.width: 1

                Row {
                    anchors.fill: parent
                    anchors.margins: 3
                    spacing: 3

                    Repeater {
                        model: [
                            { label: "2.5s", ms: 2500 },
                            { label: "5s",   ms: 5000 },
                            { label: "Never", ms: 0 }
                        ]
                        delegate: Rectangle {
                            width: (parent.width - 6) / 3
                            height: parent.height
                            radius: 7
                            property bool isSel: settingsManager.autoHideDelayMs === modelData.ms
                            color: isSel ? Theme.accent : (segHover.hovered ? Theme.surfaceHover : "transparent")

                            scale: segMouse.pressed ? 0.98 : 1.0
                            Behavior on scale { NumberAnimation { duration: 100 } }
                            Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                            HoverHandler {
                                id: segHover
                                blocking: false
                            }

                            Text {
                                anchors.centerIn: parent
                                text: modelData.label
                                font.pixelSize: 11
                                font.weight: isSel ? Font.DemiBold : Font.Normal
                                color: isSel ? Theme.textOnAccent : (segHover.hovered ? Theme.textPrimary : Theme.textSecondary)
                                Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }
                            }

                            MouseArea {
                                id: segMouse
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: settingsManager.autoHideDelayMs = modelData.ms
                            }
                        }
                    }
                }
            }
        }

        // ── 3. Theme Status Line ──────────────────────────────────────────────
        Item {
            width: parent.width
            height: 16

            Text {
                anchors.centerIn: parent
                text: {
                    var v = settingsManager.vibe;
                    var name = "Match wallpaper";
                    if (v === "obsidian") name = "Obsidian";
                    else if (v === "porcelain") name = "Porcelain";
                    else if (v === "mist") name = "Mist";
                    return "Theme · " + name;
                }
                font.pixelSize: 11
                color: Theme.textMuted
            }
        }
    }
}
