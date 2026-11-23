import QtQuick
import QtQuick.Controls
import EdgeDrop

Item {
    id: root

    // Keyboard shortcut: ⌘C / Ctrl+C to copy file
    Item {
        focus: true
        Keys.onPressed: (event) => {
            if ((event.modifiers & Qt.ControlModifier) && event.key === Qt.Key_C) {
                appController.copyResultToClipboard();
                event.accepted = true;
            }
        }
    }

    Column {
        anchors.fill: parent
        spacing: 10

        // ── Processing State ──────────────────────────────────────────────────
        Item {
            visible: appController.isProcessing
            width: parent.width
            height: visible ? parent.height : 0

            Column {
                anchors.centerIn: parent
                spacing: 12

                // Sleek Circular Progress Arc
                Canvas {
                    id: progressCanvas
                    width: 48
                    height: 48
                    anchors.horizontalCenter: parent.horizontalCenter

                    property double progress: appController.compressionProgress

                    onProgressChanged: requestPaint()

                    onPaint: {
                        var ctx = getContext("2d");
                        ctx.clearRect(0, 0, width, height);

                        var cx = width / 2;
                        var cy = height / 2;
                        var r = 20;

                        // Background track
                        ctx.beginPath();
                        ctx.arc(cx, cy, r, 0, 2 * Math.PI);
                        ctx.strokeStyle = Theme.borderHairline;
                        ctx.lineWidth = 3;
                        ctx.stroke();

                        // Progress arc
                        var startAngle = -Math.PI / 2;
                        var endAngle = startAngle + (Math.max(0.06, progress) * 2 * Math.PI);
                        ctx.beginPath();
                        ctx.arc(cx, cy, r, startAngle, endAngle);
                        ctx.strokeStyle = Theme.accent;
                        ctx.lineWidth = 3;
                        ctx.lineCap = "round";
                        ctx.stroke();
                    }
                }

                // Everyday human progress label
                Text {
                    text: appController.batchTotalVal > 1
                          ? "Shrinking " + appController.batchProgressVal + " of " + appController.batchTotalVal + " files…"
                          : (appController.compressionProgress > 0
                             ? "Shrinking your file (" + Math.round(appController.compressionProgress * 100) + "%)…"
                             : "Shrinking your file…")
                    font.pixelSize: 12
                    font.weight: Font.Medium
                    color: Theme.textPrimary
                    anchors.horizontalCenter: parent.horizontalCenter
                }

                // Stop button
                Rectangle {
                    width: 68
                    height: 26
                    radius: 7
                    color: stopHover.hovered ? Theme.surfaceHover : Theme.surfaceElevated
                    border.color: Theme.borderHairline
                    border.width: 1
                    anchors.horizontalCenter: parent.horizontalCenter

                    scale: stopMouse.pressed ? 0.98 : 1.0
                    Behavior on scale { NumberAnimation { duration: 100 } }
                    Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                    HoverHandler {
                        id: stopHover
                        blocking: false
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "Stop"
                        font.pixelSize: 11
                        font.weight: Font.Medium
                        color: Theme.danger
                    }

                    MouseArea {
                        id: stopMouse
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: appController.cancelCurrent()
                    }
                }
            }
        }

        // ── Done State ────────────────────────────────────────────────────────
        Column {
            visible: !appController.isProcessing && Boolean(appController.currentResult.success)
            width: parent.width
            spacing: 10

            // Bold Tabular Savings Headline + Soft Emerald Savings Badge
            Rectangle {
                width: parent.width
                height: 48
                radius: 10
                color: Theme.surfaceElevated
                border.color: Theme.borderHairline
                border.width: 1

                Row {
                    anchors.fill: parent
                    anchors.margins: 10
                    spacing: 8

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - badgeRect.width - 8
                        spacing: 2

                        Row {
                            spacing: 6
                            Text {
                                text: appController.currentResult.formattedInputSize || ""
                                font.pixelSize: 11
                                font.family: Theme.monoFont
                                color: Theme.textMuted
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: "→"
                                font.pixelSize: 11
                                color: Theme.textSecondary
                                anchors.verticalCenter: parent.verticalCenter
                            }
                            Text {
                                text: appController.currentResult.formattedOutputSize || ""
                                font.pixelSize: 14
                                font.weight: Font.Bold
                                font.family: Theme.monoFont
                                color: Theme.textPrimary
                                anchors.verticalCenter: parent.verticalCenter
                            }
                        }

                        Text {
                            text: appController.currentResult.outputFileName || ""
                            font.pixelSize: 10
                            color: Theme.textSecondary
                            elide: Text.ElideMiddle
                            width: parent.width
                        }
                    }

                    // Soft emerald savings badge
                    Rectangle {
                        id: badgeRect
                        width: 50
                        height: 24
                        radius: 6
                        color: Theme.successSurface
                        border.color: Theme.success
                        border.width: 1
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            anchors.centerIn: parent
                            text: "-" + Math.round(parseFloat(appController.currentResult.savingsPercentage || "0")) + "%"
                            font.pixelSize: 11
                            font.weight: Font.Bold
                            font.family: Theme.monoFont
                            color: Theme.success
                        }
                    }
                }
            }

            // Plain-English Readability Floor or Pass-Through Note
            Rectangle {
                width: parent.width
                height: 26
                radius: 6
                color: Theme.surfaceElevated
                visible: Boolean(appController.currentResult.floorNote)
                clip: true

                Row {
                    anchors.centerIn: parent
                    spacing: 5
                    width: parent.width - 12

                    Text {
                        text: "✓"
                        font.pixelSize: 10
                        font.weight: Font.Bold
                        color: Theme.accent
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        text: appController.currentResult.floorNote || ""
                        font.pixelSize: 10
                        color: Theme.textSecondary
                        elide: Text.ElideRight
                        width: parent.width - 18
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            // Elevated Drag-Out Card (54dp height, radius 12, Theme.surfaceElevated)
            Rectangle {
                id: dragOutBox
                width: parent.width
                height: 54
                radius: 12
                color: dragBoxHover.hovered ? Theme.elevatedHover : Theme.surfaceElevated
                border.color: dragBoxHover.hovered ? Theme.activeBorder : Theme.borderHairline
                border.width: 1

                scale: dragBoxMouse.pressed ? 0.98 : 1.0
                Behavior on scale { NumberAnimation { duration: 100 } }
                Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }
                Behavior on border.color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                HoverHandler {
                    id: dragBoxHover
                    blocking: false
                }

                Row {
                    anchors.centerIn: parent
                    spacing: 10

                    // 6-Dot Grip Handle (2 columns of 3 dots)
                    Grid {
                        columns: 2
                        rows: 3
                        spacing: 2.5
                        anchors.verticalCenter: parent.verticalCenter

                        Repeater {
                            model: 6
                            delegate: Rectangle {
                                width: 3
                                height: 3
                                radius: 1.5
                                color: dragBoxHover.hovered ? Theme.accent : Theme.textMuted
                                opacity: 0.8
                            }
                        }
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 2

                        Text {
                            text: appController.currentResult.outputFileName || "Compressed file"
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                            elide: Text.ElideMiddle
                            width: 200
                        }

                        Text {
                            text: "Drag anywhere to upload"
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            color: Theme.accent
                        }
                    }
                }

                MouseArea {
                    id: dragBoxMouse
                    anchors.fill: parent
                    cursorShape: Qt.OpenHandCursor
                    onPressed: appController.startDragOut()
                }
            }

            // ── Clean Copy · Show · Done Action Bar (36dp height, 8dp spacing) ──
            Row {
                width: parent.width
                height: 36
                spacing: 8

                // Copy
                Rectangle {
                    width: (parent.width - 16) / 3
                    height: parent.height
                    radius: Theme.radiusSmall
                    color: copyHover.hovered ? Theme.elevatedHover : Theme.surfaceElevated
                    border.color: copyHover.hovered ? Theme.activeBorder : Theme.borderHairline
                    border.width: 1

                    scale: copyMouse.pressed ? 0.98 : 1.0
                    Behavior on scale { NumberAnimation { duration: 100 } }
                    Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }
                    Behavior on border.color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                    HoverHandler {
                        id: copyHover
                        blocking: false
                    }

                    Row {
                        anchors.centerIn: parent
                        spacing: 4

                        TintedIcon {
                            size: 12
                            source: "qrc:/qt/qml/EdgeDrop/resources/icons/copy.svg"
                            color: copyHover.hovered ? Theme.textPrimary : Theme.textSecondary
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: "Copy"
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            color: copyHover.hovered ? Theme.textPrimary : Theme.textSecondary
                            anchors.verticalCenter: parent.verticalCenter
                            Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }
                        }
                    }

                    AppToolTip {
                        visible: copyHover.hovered
                        text: "Copy to clipboard"
                    }

                    MouseArea {
                        id: copyMouse
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: appController.copyResultToClipboard()
                    }
                }

                // Show
                Rectangle {
                    width: (parent.width - 16) / 3
                    height: parent.height
                    radius: Theme.radiusSmall
                    color: showHover.hovered ? Theme.elevatedHover : Theme.surfaceElevated
                    border.color: showHover.hovered ? Theme.activeBorder : Theme.borderHairline
                    border.width: 1

                    scale: showMouse.pressed ? 0.98 : 1.0
                    Behavior on scale { NumberAnimation { duration: 100 } }
                    Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }
                    Behavior on border.color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                    HoverHandler {
                        id: showHover
                        blocking: false
                    }

                    Row {
                        anchors.centerIn: parent
                        spacing: 4

                        TintedIcon {
                            size: 12
                            source: "qrc:/qt/qml/EdgeDrop/resources/icons/folder.svg"
                            color: showHover.hovered ? Theme.textPrimary : Theme.textSecondary
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: "Show"
                            font.pixelSize: 11
                            font.weight: Font.Medium
                            color: showHover.hovered ? Theme.textPrimary : Theme.textSecondary
                            anchors.verticalCenter: parent.verticalCenter
                            Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }
                        }
                    }

                    AppToolTip {
                        visible: showHover.hovered
                        text: "Reveal in folder"
                    }

                    MouseArea {
                        id: showMouse
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: appController.showResultInFolder()
                    }
                }

                // Done
                Rectangle {
                    width: (parent.width - 16) / 3
                    height: parent.height
                    radius: Theme.radiusSmall
                    color: doneHover.hovered ? Theme.accentHover : Theme.accent

                    scale: newMouse.pressed ? 0.98 : 1.0
                    Behavior on scale { NumberAnimation { duration: 100 } }
                    Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                    HoverHandler {
                        id: doneHover
                        blocking: false
                    }

                    Row {
                        anchors.centerIn: parent
                        spacing: 4

                        TintedIcon {
                            size: 12
                            source: "qrc:/qt/qml/EdgeDrop/resources/icons/check.svg"
                            color: Theme.textOnAccent
                            anchors.verticalCenter: parent.verticalCenter
                        }

                        Text {
                            text: "Done"
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            color: Theme.textOnAccent
                            anchors.verticalCenter: parent.verticalCenter
                        }
                    }

                    AppToolTip {
                        visible: doneHover.hovered
                        text: "Shrink another file"
                    }

                    MouseArea {
                        id: newMouse
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: appController.resetToHome()
                    }
                }
            }
        }
    }
}
