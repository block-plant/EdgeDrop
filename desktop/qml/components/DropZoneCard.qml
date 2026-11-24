import QtQuick
import QtQuick.Controls
import EdgeDrop

Item {
    id: root

    implicitHeight: mainCol.implicitHeight

    Column {
        id: mainCol
        anchors.fill: parent
        spacing: 12

        // Clean Dashed Drop Zone
        Rectangle {
            id: dropZone
            width: parent.width
            implicitHeight: dropContentCol.implicitHeight + 40
            height: recentRowCol.visible
                    ? Math.max(implicitHeight, parent.height - recentRowCol.implicitHeight - 12)
                    : Math.max(implicitHeight, parent.height)
            radius: Theme.radiusInner
            color: rootDropArea.containsDrag ? Theme.accentSurface : (dropZoneHover.hovered ? Theme.elevatedHover : Theme.surfaceElevated)
            border.color: rootDropArea.containsDrag ? Theme.accent : (dropZoneHover.hovered ? Theme.activeBorder : Theme.borderHairline)
            border.width: 1.5

            Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }
            Behavior on border.color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

            HoverHandler {
                id: dropZoneHover
                blocking: false
            }

            Column {
                id: dropContentCol
                anchors.centerIn: parent
                spacing: 8
                width: parent.width - 36

                // Centred 36x36dp soft icon badge with 18dp cloud/drop icon
                Rectangle {
                    width: 36
                    height: 36
                    radius: 18
                    anchors.horizontalCenter: parent.horizontalCenter
                    color: rootDropArea.containsDrag ? Theme.accent : (dropZoneHover.hovered ? Theme.elevatedHover : Theme.pillSurface)
                    scale: rootDropArea.containsDrag ? 1.04 : (dropZoneMouse.pressed ? 0.98 : 1.0)
                    Behavior on scale { NumberAnimation { duration: 100 } }
                    Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                    TintedIcon {
                        anchors.centerIn: parent
                        size: 18
                        source: "qrc:/qt/qml/EdgeDrop/resources/icons/drop.svg"
                        color: rootDropArea.containsDrag ? "#FFFFFF" : (dropZoneHover.hovered ? Theme.textPrimary : Theme.textSecondary)
                    }
                }

                // Everyday human labels
                Column {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 3

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Drop any file here"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        font.letterSpacing: -0.2
                        color: Theme.textPrimary
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "Photos · PDF · Docs · Video · Audio"
                        font.pixelSize: 11
                        color: Theme.textSecondary
                    }

                    Text {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: "JPG · PNG · WebP · HEIC · PDF · DOCX · PPTX\nMP4 · MOV · MP3 · M4A · WAV · Up to 2 GB"
                        font.pixelSize: 9
                        color: Theme.textMuted
                        horizontalAlignment: Text.AlignHCenter
                        lineHeight: 1.3
                    }
                }

                // Action links
                Row {
                    anchors.horizontalCenter: parent.horizontalCenter
                    spacing: 8

                    Text {
                        text: "Choose file"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        color: chooseHover.hovered ? Theme.accentHover : Theme.accent
                        font.underline: chooseHover.hovered
                        Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                        HoverHandler {
                            id: chooseHover
                            blocking: false
                        }

                        MouseArea {
                            id: chooseMouse
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: appController.chooseFile()
                        }
                    }

                    Text {
                        text: "·"
                        font.pixelSize: 12
                        color: Theme.textSecondary
                    }

                    Text {
                        text: "Paste"
                        font.pixelSize: 12
                        font.weight: Font.Medium
                        color: pasteHover.hovered ? Theme.accentHover : Theme.accent
                        font.underline: pasteHover.hovered
                        Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                        HoverHandler {
                            id: pasteHover
                            blocking: false
                        }

                        AppToolTip {
                            visible: pasteHover.hovered
                            text: "⌘V / Ctrl+V"
                        }

                        MouseArea {
                            id: pasteMouse
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: appController.handleClipboardPaste()
                        }
                    }
                }
            }

            MouseArea {
                id: dropZoneMouse
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: appController.chooseFile()
            }
        }

        // Recent (3) Draggable Chips
        Column {
            id: recentRowCol
            width: parent.width
            spacing: 6
            visible: appController.recentFiles.length > 0

            Text {
                id: recentLabel
                text: "Recent"
                font.pixelSize: 11
                font.weight: Font.Medium
                color: Theme.textMuted
            }

            Row {
                width: parent.width
                spacing: 6

                Repeater {
                    model: appController.recentFiles
                    delegate: Rectangle {
                        width: (parent.width - ((appController.recentFiles.length - 1) * 6)) / Math.max(1, appController.recentFiles.length)
                        height: 28
                        radius: Theme.radiusSmall
                        color: chipHover.hovered ? Theme.elevatedHover : Theme.surfaceElevated
                        border.color: chipHover.hovered ? Theme.activeBorder : Theme.borderHairline
                        border.width: 1

                        scale: recentChipMouse.pressed ? 0.98 : 1.0
                        Behavior on scale { NumberAnimation { duration: 100 } }
                        Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }
                        Behavior on border.color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                        HoverHandler {
                            id: chipHover
                            blocking: false
                        }

                        Row {
                            anchors.centerIn: parent
                            spacing: 5
                            width: parent.width - 10

                            TintedIcon {
                                size: 12
                                source: "qrc:/qt/qml/EdgeDrop/resources/icons/document.svg"
                                color: chipHover.hovered ? Theme.textPrimary : Theme.textSecondary
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Text {
                                text: modelData.fileName || ""
                                font.pixelSize: 11
                                font.family: Theme.monoFont
                                color: chipHover.hovered ? Theme.textPrimary : Theme.textSecondary
                                elide: Text.ElideMiddle
                                width: parent.width - 20
                                anchors.verticalCenter: parent.verticalCenter
                                Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }
                            }
                        }

                        AppToolTip {
                            visible: chipHover.hovered
                            text: "Drag or click to share"
                        }

                        MouseArea {
                            id: recentChipMouse
                            anchors.fill: parent
                            cursorShape: Qt.OpenHandCursor
                            onClicked: appController.startDragFile(modelData.path)
                            onPressed: appController.startDragFile(modelData.path)
                        }
                    }
                }
            }
        }
    }
}
