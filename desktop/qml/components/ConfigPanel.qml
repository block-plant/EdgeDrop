import QtQuick
import QtQuick.Controls
import EdgeDrop

Item {
    id: root

    property int targetNumber: 500
    property string targetUnit: "KB"

    function getOriginalBytes() {
        return appController.currentFileInfo.sizeBytes || 0;
    }

    function computeChips() {
        var bytes = getOriginalBytes();
        if (bytes < 2 * 1024 * 1024) {
            return [
                { label: "50 KB",  val: 50,  unit: "KB" },
                { label: "100 KB", val: 100, unit: "KB" },
                { label: "200 KB", val: 200, unit: "KB" },
                { label: "500 KB", val: 500, unit: "KB" }
            ];
        } else if (bytes <= 15 * 1024 * 1024) {
            return [
                { label: "200 KB", val: 200, unit: "KB" },
                { label: "500 KB", val: 500, unit: "KB" },
                { label: "1 MB",   val: 1,   unit: "MB" },
                { label: "2 MB",   val: 2,   unit: "MB" }
            ];
        } else {
            return [
                { label: "5 MB",  val: 5,  unit: "MB" },
                { label: "10 MB", val: 10, unit: "MB" },
                { label: "25 MB", val: 25, unit: "MB" },
                { label: "50 MB", val: 50, unit: "MB" }
            ];
        }
    }

    function syncTargetBytes() {
        if (targetUnit === "MB") {
            appController.setTargetMb(targetNumber);
        } else {
            appController.setTargetKb(targetNumber);
        }
    }

    Component.onCompleted: {
        var chips = computeChips();
        if (chips.length > 1) {
            targetNumber = chips[1].val;
            targetUnit = chips[1].unit;
        }
        syncTargetBytes();
        targetInput.forceActiveFocus();
    }

    Column {
        anchors.fill: parent
        spacing: 10

        // ── 1. Uploaded File Strip(s) ─────────────────────────────────────────
        Column {
            width: parent.width
            spacing: 4

            // Single file strip (40dp height, radius 10, Theme.surfaceElevated)
            Rectangle {
                width: parent.width
                height: 40
                radius: 10
                color: Theme.surfaceElevated
                border.color: Theme.borderHairline
                border.width: 1
                visible: appController.loadedFiles.length <= 1

                Row {
                    anchors.left: parent.left
                    anchors.leftMargin: 8
                    anchors.right: closeBtn.left
                    anchors.rightMargin: 6
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    Rectangle {
                        width: 24
                        height: 24
                        radius: 6
                        color: Theme.accentSurface
                        border.color: Theme.accentBorder
                        border.width: 1
                        anchors.verticalCenter: parent.verticalCenter

                        TintedIcon {
                            anchors.centerIn: parent
                            size: 13
                            source: "qrc:/qt/qml/EdgeDrop/resources/icons/document.svg"
                            color: Theme.accent
                        }
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - 32
                        spacing: 2

                        Text {
                            text: appController.currentFileInfo.fileName || "File"
                            font.pixelSize: 11
                            font.weight: Font.DemiBold
                            color: Theme.textPrimary
                            elide: Text.ElideMiddle
                            width: parent.width
                        }

                        Text {
                            text: appController.currentFileInfo.formattedSize || ""
                            font.pixelSize: 10
                            font.family: Theme.monoFont
                            color: Theme.textMuted
                        }
                    }
                }

                // 24x24dp circular tactile remove button
                Rectangle {
                    id: closeBtn
                    anchors.right: parent.right
                    anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 24
                    height: 24
                    radius: 12
                    color: closeHover.hovered ? Theme.surfaceHover : "transparent"

                    scale: closeBtnMouse.pressed ? 0.98 : 1.0
                    Behavior on scale { NumberAnimation { duration: 100 } }
                    Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                    HoverHandler {
                        id: closeHover
                        blocking: false
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "×"
                        font.pixelSize: 15
                        font.weight: Font.DemiBold
                        color: closeHover.hovered ? Theme.textPrimary : Theme.textSecondary
                        Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }
                    }

                    AppToolTip {
                        visible: closeHover.hovered
                        text: "Remove file"
                    }

                    MouseArea {
                        id: closeBtnMouse
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: appController.removeFileAt(0)
                    }
                }
            }

            // Multi-file batch header & items
            Column {
                width: parent.width
                spacing: 4
                visible: appController.loadedFiles.length > 1

                Item {
                    width: parent.width
                    height: 20

                    Text {
                        text: "Batch (" + appController.loadedFiles.length + " files)"
                        font.pixelSize: 11
                        font.weight: Font.DemiBold
                        color: Theme.textPrimary
                        anchors.left: parent.left
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Clear all"
                        font.pixelSize: 10
                        font.weight: Font.Medium
                        color: clearAllHover.hovered ? Theme.accentHover : Theme.accent
                        font.underline: clearAllHover.hovered
                        Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                        HoverHandler {
                            id: clearAllHover
                            blocking: false
                        }

                        MouseArea {
                            id: clearAllMouse
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: appController.clearFiles()
                        }
                    }
                }

                Repeater {
                    model: appController.loadedFiles.slice(0, 3)
                    delegate: Rectangle {
                        width: parent.width
                        height: 28
                        radius: 8
                        color: Theme.surfaceElevated
                        border.color: Theme.borderHairline
                        border.width: 1

                        Item {
                            anchors.fill: parent
                            anchors.margins: 4

                            Text {
                                text: modelData.split('/').pop()
                                font.pixelSize: 10
                                color: Theme.textPrimary
                                elide: Text.ElideMiddle
                                width: parent.width - 24
                                anchors.left: parent.left
                                anchors.verticalCenter: parent.verticalCenter
                            }

                            Rectangle {
                                width: 20
                                height: 20
                                radius: 10
                                color: itemRemoveHover.hovered ? Theme.surfaceHover : "transparent"
                                anchors.right: parent.right
                                anchors.verticalCenter: parent.verticalCenter

                                scale: itemRemoveMouse.pressed ? 0.98 : 1.0
                                Behavior on scale { NumberAnimation { duration: 100 } }
                                Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                                HoverHandler {
                                    id: itemRemoveHover
                                    blocking: false
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: "×"
                                    font.pixelSize: 13
                                    color: itemRemoveHover.hovered ? Theme.textPrimary : Theme.textSecondary
                                    Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }
                                }

                                MouseArea {
                                    id: itemRemoveMouse
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: appController.removeFileAt(index)
                                }
                            }
                        }
                    }
                }
            }
        }

        // ── 2. Hero Target Input ("Keep under") ────────────────────────────────
        Column {
            width: parent.width
            spacing: 6

            Text {
                text: "Keep under"
                font.pixelSize: 11
                font.weight: Font.Medium
                color: Theme.textSecondary
            }

            Rectangle {
                width: parent.width
                height: 44
                radius: 10
                color: Theme.surfaceElevated
                border.color: targetInput.activeFocus ? Theme.accent : Theme.borderHairline
                border.width: targetInput.activeFocus ? 1.5 : 1

                Row {
                    anchors.fill: parent
                    anchors.margins: 8
                    spacing: 8

                    TextInput {
                        id: targetInput
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width - segSwitch.width - 12
                        font.pixelSize: 20
                        font.weight: Font.DemiBold
                        font.family: Theme.monoFont
                        color: Theme.textPrimary
                        text: root.targetNumber.toString()
                        selectByMouse: true
                        validator: IntValidator { bottom: 1; top: 9999 }
                        onTextChanged: {
                            var n = parseInt(text);
                            if (!isNaN(n) && n > 0) {
                                root.targetNumber = n;
                                root.syncTargetBytes();
                            }
                        }
                        onAccepted: appController.processCurrent()
                    }

                    // Segmented KB | MB switch
                    Rectangle {
                        id: segSwitch
                        anchors.verticalCenter: parent.verticalCenter
                        width: 76
                        height: 28
                        radius: 6
                        color: Theme.pillSurface
                        border.color: Theme.borderHairline
                        border.width: 1

                        Row {
                            anchors.fill: parent
                            anchors.margins: 2
                            spacing: 2

                            Rectangle {
                                width: (parent.width - 2) / 2
                                height: parent.height
                                radius: 4
                                color: root.targetUnit === "KB" ? Theme.accent : (kbHover.hovered ? Theme.surfaceHover : "transparent")

                                scale: kbMouse.pressed ? 0.98 : 1.0
                                Behavior on scale { NumberAnimation { duration: 100 } }
                                Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                                HoverHandler {
                                    id: kbHover
                                    blocking: false
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: "KB"
                                    font.pixelSize: 10
                                    font.weight: root.targetUnit === "KB" ? Font.Bold : Font.Normal
                                    color: root.targetUnit === "KB" ? Theme.textOnAccent : (kbHover.hovered ? Theme.textPrimary : Theme.textSecondary)
                                    Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }
                                }

                                MouseArea {
                                    id: kbMouse
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        root.targetUnit = "KB";
                                        root.syncTargetBytes();
                                    }
                                }
                            }

                            Rectangle {
                                width: (parent.width - 2) / 2
                                height: parent.height
                                radius: 4
                                color: root.targetUnit === "MB" ? Theme.accent : (mbHover.hovered ? Theme.surfaceHover : "transparent")

                                scale: mbMouse.pressed ? 0.98 : 1.0
                                Behavior on scale { NumberAnimation { duration: 100 } }
                                Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                                HoverHandler {
                                    id: mbHover
                                    blocking: false
                                }

                                Text {
                                    anchors.centerIn: parent
                                    text: "MB"
                                    font.pixelSize: 10
                                    font.weight: root.targetUnit === "MB" ? Font.Bold : Font.Normal
                                    color: root.targetUnit === "MB" ? Theme.textOnAccent : (mbHover.hovered ? Theme.textPrimary : Theme.textSecondary)
                                    Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }
                                }

                                MouseArea {
                                    id: mbMouse
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        root.targetUnit = "MB";
                                        root.syncTargetBytes();
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        // ── 3. Smart Adaptive Size Chips ──────────────────────────────────────
        Row {
            width: parent.width
            spacing: 6

            Repeater {
                model: root.computeChips()
                delegate: Rectangle {
                    width: (parent.width - 18) / 4
                    height: 30
                    radius: Theme.radiusSmall
                    property bool isSelected: root.targetNumber === modelData.val && root.targetUnit === modelData.unit
                    color: isSelected ? Theme.accent : (sizeChipHover.hovered ? Theme.elevatedHover : Theme.surfaceElevated)
                    border.color: isSelected ? Theme.accent : (sizeChipHover.hovered ? Theme.activeBorder : Theme.borderHairline)
                    border.width: 1

                    scale: chipMouse.pressed ? 0.98 : 1.0
                    Behavior on scale { NumberAnimation { duration: 100 } }
                    Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }
                    Behavior on border.color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                    HoverHandler {
                        id: sizeChipHover
                        blocking: false
                    }

                    Text {
                        anchors.centerIn: parent
                        text: modelData.label
                        font.pixelSize: 10
                        font.weight: isSelected ? Font.DemiBold : Font.Normal
                        color: isSelected ? Theme.textOnAccent : (sizeChipHover.hovered ? Theme.textPrimary : Theme.textSecondary)
                        Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }
                    }

                    MouseArea {
                        id: chipMouse
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.targetNumber = modelData.val;
                            root.targetUnit = modelData.unit;
                            targetInput.text = modelData.val.toString();
                            root.syncTargetBytes();
                        }
                    }
                }
            }
        }

        // ── 4. Full-Width High-Contrast Compress CTA ──────────────────────────
        Rectangle {
            id: compressBtn
            width: parent.width
            height: 40
            radius: 10
            opacity: appController.isProcessing ? 0.6 : 1.0
            color: (compressHover.hovered && !appController.isProcessing) ? Theme.accentHover : Theme.accent

            scale: compressMouse.pressed ? 0.98 : 1.0
            Behavior on scale { NumberAnimation { duration: 100 } }
            Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }
            Behavior on opacity { NumberAnimation { duration: 120 } }

            HoverHandler {
                id: compressHover
                blocking: false
            }

            Row {
                anchors.centerIn: parent
                spacing: 6

                Text {
                    text: appController.isProcessing ? "Shrinking…" : "Compress"
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: Theme.textOnAccent
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    visible: !appController.isProcessing
                    text: "↵"
                    font.pixelSize: 11
                    color: Qt.rgba(1, 1, 1, 0.75)
                    anchors.verticalCenter: parent.verticalCenter
                }
            }

            MouseArea {
                id: compressMouse
                anchors.fill: parent
                enabled: !appController.isProcessing
                cursorShape: appController.isProcessing ? Qt.BusyCursor : Qt.PointingHandCursor
                onClicked: {
                    mainWindow.requestActivate();
                    appController.processCurrent();
                }
            }
        }

        // ── 5. Quiet Clickable Saves-to Footer ─────────────────────────────────
        Item {
            anchors.horizontalCenter: parent.horizontalCenter
            width: footerRow.width
            height: 16

            HoverHandler {
                id: footerHover
                blocking: false
            }

            Row {
                id: footerRow
                anchors.centerIn: parent
                spacing: 4

                TintedIcon {
                    size: 11
                    source: "qrc:/qt/qml/EdgeDrop/resources/icons/folder.svg"
                    color: footerHover.hovered ? Theme.textPrimary : Theme.textMuted
                    anchors.verticalCenter: parent.verticalCenter
                }

                Text {
                    text: "Saves to " + (settingsManager.outputDirectory.split('/').pop() || "Downloads")
                    font.pixelSize: 11
                    color: footerHover.hovered ? Theme.textPrimary : Theme.textMuted
                    font.underline: footerHover.hovered
                    anchors.verticalCenter: parent.verticalCenter
                    Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }
                }
            }

            MouseArea {
                id: footerMouse
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: appController.chooseOutputFolder()
            }
        }
    }
}
