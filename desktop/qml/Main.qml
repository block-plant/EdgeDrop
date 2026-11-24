import QtQuick
import QtQuick.Controls
import QtQuick.Window
import EdgeDrop

Window {
    id: mainWindow
    visible: true
    flags: Qt.Window | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: "transparent"

    property bool isDragActive: rootDropArea.containsDrag
    property bool settingsOpen: false

    // Strict mutually exclusive active view router
    readonly property string activeView: settingsOpen ? "settings"
                                       : (appController.isProcessing || (appController.currentResult && appController.currentResult.hasOwnProperty("success"))) ? "result"
                                       : (appController.loadedFiles && appController.loadedFiles.length > 0) ? "config"
                                       : "home"

    width: edgeWindowManager.windowWidth > 0 ? edgeWindowManager.windowWidth : 14
    height: edgeWindowManager.windowHeight > 0 ? edgeWindowManager.windowHeight : 68
    x: edgeWindowManager.windowX
    y: edgeWindowManager.windowY

    function updateActiveCardHeight() {
        if (!edgeWindowManager.isExpanded) return;
        var h = 246;
        if (activeView === "settings") {
            h = 264;
        } else if (activeView === "home") {
            h = (appController.recentFiles && appController.recentFiles.length > 0) ? 306 : 246;
        } else if (activeView === "config") {
            var batchCount = appController.loadedFiles ? appController.loadedFiles.length : 0;
            var baseHeight = (appController.isCurrentFileMedia && !appController.hasFfmpeg) ? 376 : 292;
            if (batchCount > 1) {
                h = Math.min(460, baseHeight + (batchCount - 1) * 32);
            } else {
                h = baseHeight;
            }
        } else if (activeView === "result") {
            if (appController.isProcessing) {
                h = 220;
            } else if (appController.currentResult && appController.currentResult.success === false) {
                h = 210;
            } else if (appController.resultHitFloor || (appController.floorNote && appController.floorNote.length > 0)) {
                h = 268;
            } else {
                h = 232;
            }
        }
        edgeWindowManager.setActiveViewAutoHeight(h);
    }

    onActiveViewChanged: updateActiveCardHeight()

    property bool isAutoHideEligible: (activeView === "home" || activeView === "result")

    // Auto-hide timer (paused during repositioning or pin)
    Timer {
        id: autoHideTimer
        interval: settingsManager.autoHideDelayMs > 0 ? settingsManager.autoHideDelayMs : 2500
        repeat: false
        onTriggered: {
            if (!settingsManager.isPinned &&
                !appController.isProcessing &&
                !edgeWindowManager.isRepositioning &&
                !edgeWindowManager.isSnapping &&
                !edgeWindowManager.isResizing &&
                isAutoHideEligible &&
                !cardHoverHandler.hovered) {
                edgeWindowManager.isExpanded = false;
            }
        }
    }

    Connections {
        target: edgeWindowManager
        function onIsExpandedChanged() {
            if (edgeWindowManager.isExpanded) {
                mainWindow.updateActiveCardHeight();
                if (!cardHoverHandler.hovered && !settingsManager.isPinned && !appController.isProcessing && isAutoHideEligible) {
                    autoHideTimer.restart();
                }
            } else {
                autoHideTimer.stop();
            }
        }
    }

    Connections {
        target: appController
        function onResetToHomeRequested() {
            settingsOpen = false;
            mainWindow.updateActiveCardHeight();
        }
        function onCurrentResultChanged() {
            mainWindow.updateActiveCardHeight();
        }
        function onIsProcessingChanged() {
            mainWindow.updateActiveCardHeight();
        }
        function onFfmpegAvailabilityChanged() {
            mainWindow.updateActiveCardHeight();
        }
        function onErrorOccurred(message) {
            expandedContent.lastErrorMessage = message;
            expandedContent.showErrorBanner = true;
            errorBannerTimer.restart();
        }
    }

    // Global Shortcuts
    Shortcut {
        sequence: "Escape"
        onActivated: {
            if (settingsOpen) {
                settingsOpen = false;
            } else {
                edgeWindowManager.isExpanded = false;
            }
        }
    }
    Shortcut {
        sequence: "Return"
        onActivated: {
            if (activeView === "config" && !appController.isProcessing) {
                appController.processCurrent();
            }
        }
    }
    Shortcut {
        sequence: "Enter"
        onActivated: {
            if (activeView === "config" && !appController.isProcessing) {
                appController.processCurrent();
            }
        }
    }

    // Root-level DropArea covering the entire window
    DropArea {
        id: rootDropArea
        anchors.fill: parent
        z: 999
        onEntered: (drag) => {
            edgeWindowManager.isExpanded = true;
            mainWindow.updateActiveCardHeight();
        }
        onDropped: (drop) => {
            if (drop.hasUrls) {
                settingsOpen = false;
                appController.loadFiles(drop.urls);
                drop.acceptProposedAction();
            }
        }
    }

    // Idle Pill Handle (Direct sibling under Window)
    EdgeHandle {
        id: edgeHandle
        anchors.fill: parent
        visible: !edgeWindowManager.isExpanded && !edgeWindowManager.isSnappingToExpanded
        z: 100
        onTriggered: {
            edgeWindowManager.isExpanded = true;
            mainWindow.updateActiveCardHeight();
        }
    }

    // Main Card Surface (Visible strictly when expanded)
    Rectangle {
        id: cardSurface
        anchors.fill: parent
        visible: edgeWindowManager.isExpanded || opacity > 0.01
        opacity: edgeWindowManager.isExpanded ? 1.0 : 0.0
        radius: edgeWindowManager.isExpanded ? Theme.radiusOuter : Theme.radiusPill
        color: edgeWindowManager.isExpanded ? Theme.cardSurface : "transparent"
        border.color: edgeWindowManager.isExpanded ? Theme.borderHairline : "transparent"
        border.width: edgeWindowManager.isExpanded ? 1 : 0
        clip: true
        layer.enabled: false

        Behavior on opacity {
            NumberAnimation {
                duration: edgeWindowManager.isExpanded ? 160 : Theme.animCollapseMs
                easing.type: Easing.OutCubic
            }
        }
        Behavior on color { ColorAnimation { duration: Theme.animColorMs } }
        Behavior on radius { NumberAnimation { duration: Theme.animExpandMs; easing.type: Easing.OutQuint } }

        // Hover detection on expanded card for auto-hide
        HoverHandler {
            id: cardHoverHandler
            onHoveredChanged: {
                if (hovered) {
                    autoHideTimer.stop();
                } else {
                    if (edgeWindowManager.isExpanded &&
                        !settingsManager.isPinned &&
                        !appController.isProcessing &&
                        !edgeWindowManager.isRepositioning &&
                        !edgeWindowManager.isSnapping &&
                        !edgeWindowManager.isResizing &&
                        mainWindow.isAutoHideEligible &&
                        settingsManager.autoHideDelayMs > 0) {
                        autoHideTimer.restart();
                    }
                }
            }
        }

        // Expanded Card Content
        Item {
            id: expandedContent
            anchors.fill: parent
            visible: cardSurface.visible && mainWindow.width >= 300
            opacity: (edgeWindowManager.isExpanded && mainWindow.width >= 300) ? 1.0 : 0.0

            Behavior on opacity {
                NumberAnimation {
                    duration: edgeWindowManager.isExpanded ? 160 : Theme.animCollapseMs
                    easing.type: Easing.OutCubic
                }
            }

            // ── Top Utility Bar (32dp height, 12dp topMargin, 14dp side margins) ──
            Item {
                id: topBar
                anchors.top: parent.top
                anchors.topMargin: 12
                anchors.left: parent.left
                anchors.leftMargin: 14
                anchors.right: parent.right
                anchors.rightMargin: 14
                height: 32
                z: 300

                // Left: Home button (when activeView !== "home") + 4 Vibe Dots
                Row {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    // ← Home Pill Button
                    Rectangle {
                        id: homeBackBtn
                        width: 24
                        height: 24
                        radius: 6
                        visible: mainWindow.activeView !== "home"
                        color: homeHover.hovered ? Theme.surfaceHover : "transparent"
                        border.color: homeHover.hovered ? Theme.borderHairline : "transparent"
                        border.width: 1
                        anchors.verticalCenter: parent.verticalCenter

                        scale: homeMouse.pressed ? 0.98 : 1.0
                        Behavior on scale { NumberAnimation { duration: 100 } }
                        Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }
                        Behavior on border.color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                        HoverHandler {
                            id: homeHover
                            blocking: false
                        }

                        Text {
                            anchors.centerIn: parent
                            text: "←"
                            font.pixelSize: 13
                            font.weight: Font.DemiBold
                            color: homeHover.hovered ? Theme.textPrimary : Theme.textSecondary
                            Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }
                        }

                        AppToolTip {
                            visible: homeHover.hovered
                            text: "Home"
                        }

                        MouseArea {
                            id: homeMouse
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                mainWindow.settingsOpen = false;
                                appController.resetToHome();
                            }
                        }
                    }

                    // 4 Vibe Dots (11dp circles, 7dp spacing, subtle ring opacity lift 0.35 -> 0.75 on hover)
                    Row {
                        id: vibeDotsRow
                        spacing: 7
                        anchors.verticalCenter: parent.verticalCenter

                        // 1. Adaptive
                        Rectangle {
                            width: 11
                            height: 11
                            radius: 5.5
                            color: typeof wallpaperThemeEngine !== "undefined" && wallpaperThemeEngine ? wallpaperThemeEngine.accent : "#3B82F6"
                            border.width: settingsManager.vibe === "adaptive" ? 1.5 : 1.0
                            border.color: settingsManager.vibe === "adaptive"
                                ? Theme.textPrimary
                                : Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, adaptiveHover.hovered ? 0.75 : 0.35)
                            scale: adaptiveMouse.pressed ? 0.98 : 1.0
                            Behavior on scale { NumberAnimation { duration: 100 } }
                            Behavior on border.color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                            HoverHandler {
                                id: adaptiveHover
                                blocking: false
                            }

                            AppToolTip {
                                visible: adaptiveHover.hovered
                                text: "Match wallpaper"
                            }

                            MouseArea {
                                id: adaptiveMouse
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: settingsManager.vibe = "adaptive"
                            }
                        }

                        // 2. Obsidian
                        Rectangle {
                            width: 11
                            height: 11
                            radius: 5.5
                            color: "#161618"
                            border.width: settingsManager.vibe === "obsidian" ? 1.5 : 1.0
                            border.color: settingsManager.vibe === "obsidian"
                                ? Theme.textPrimary
                                : Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, obsHover.hovered ? 0.75 : 0.35)
                            scale: obsMouse.pressed ? 0.98 : 1.0
                            Behavior on scale { NumberAnimation { duration: 100 } }
                            Behavior on border.color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                            HoverHandler {
                                id: obsHover
                                blocking: false
                            }

                            AppToolTip {
                                visible: obsHover.hovered
                                text: "Obsidian"
                            }

                            MouseArea {
                                id: obsMouse
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: settingsManager.vibe = "obsidian"
                            }
                        }

                        // 3. Porcelain
                        Rectangle {
                            width: 11
                            height: 11
                            radius: 5.5
                            color: "#FAF9F6"
                            border.width: settingsManager.vibe === "porcelain" ? 1.5 : 1.0
                            border.color: settingsManager.vibe === "porcelain"
                                ? Theme.textPrimary
                                : Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, porcHover.hovered ? 0.75 : 0.35)
                            scale: porcMouse.pressed ? 0.98 : 1.0
                            Behavior on scale { NumberAnimation { duration: 100 } }
                            Behavior on border.color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                            HoverHandler {
                                id: porcHover
                                blocking: false
                            }

                            AppToolTip {
                                visible: porcHover.hovered
                                text: "Porcelain"
                            }

                            MouseArea {
                                id: porcMouse
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: settingsManager.vibe = "porcelain"
                            }
                        }

                        // 4. Mist
                        Rectangle {
                            width: 11
                            height: 11
                            radius: 5.5
                            color: Theme.isLightSurface ? Qt.rgba(0.2, 0.2, 0.25, 0.45) : Qt.rgba(0.85, 0.85, 0.9, 0.7)
                            border.width: settingsManager.vibe === "mist" ? 1.5 : 1.0
                            border.color: settingsManager.vibe === "mist"
                                ? Theme.textPrimary
                                : Qt.rgba(Theme.textPrimary.r, Theme.textPrimary.g, Theme.textPrimary.b, mistHover.hovered ? 0.75 : 0.35)
                            scale: mistMouse.pressed ? 0.98 : 1.0
                            Behavior on scale { NumberAnimation { duration: 100 } }
                            Behavior on border.color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                            HoverHandler {
                                id: mistHover
                                blocking: false
                            }

                            AppToolTip {
                                visible: mistHover.hovered
                                text: "Mist"
                            }

                            MouseArea {
                                id: mistMouse
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: settingsManager.vibe = "mist"
                            }
                        }
                    }
                }

                // Centre: Draggable Grab Pill (30dp x 4dp)
                Rectangle {
                    id: centerHandle
                    width: 30
                    height: 4
                    radius: 2
                    anchors.centerIn: parent
                    color: Theme.textMuted
                    opacity: (centerHover.hovered || edgeWindowManager.isRepositioning) ? 0.85 : 0.50
                    Behavior on opacity { NumberAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                    HoverHandler {
                        id: centerHover
                        blocking: false
                    }

                    MouseArea {
                        id: centerDragArea
                        anchors.fill: parent
                        anchors.margins: -8
                        acceptedButtons: Qt.LeftButton
                        cursorShape: edgeWindowManager.isRepositioning ? Qt.ClosedHandCursor : Qt.OpenHandCursor

                        property bool isDragging: false
                        property bool dragArmed: false
                        property int pressGlobalX: 0
                        property int pressGlobalY: 0

                        onPressed: (mouse) => {
                            if (mouse.button !== Qt.LeftButton) return;
                            isDragging = false;
                            dragArmed = true;
                            var pt = centerDragArea.mapToGlobal(mouse.x, mouse.y);
                            pressGlobalX = pt.x;
                            pressGlobalY = pt.y;
                        }

                        onPositionChanged: (mouse) => {
                            if (!dragArmed) return;
                            if ((mouse.buttons & Qt.LeftButton) === 0) {
                                if (isDragging) {
                                    var endPt = centerDragArea.mapToGlobal(mouse.x, mouse.y);
                                    edgeWindowManager.endRepositioning(endPt.x, endPt.y);
                                }
                                isDragging = false;
                                dragArmed = false;
                                return;
                            }

                            var pt = centerDragArea.mapToGlobal(mouse.x, mouse.y);
                            if (!isDragging) {
                                var dx = pt.x - pressGlobalX;
                                var dy = pt.y - pressGlobalY;
                                if (Math.hypot(dx, dy) >= 6) {
                                    isDragging = true;
                                    edgeWindowManager.startRepositioning(pt.x, pt.y);
                                }
                            } else {
                                edgeWindowManager.updateRepositioning(pt.x, pt.y);
                            }
                        }

                        onReleased: (mouse) => {
                            if (mouse.button !== Qt.LeftButton) return;
                            if (isDragging) {
                                var pt = centerDragArea.mapToGlobal(mouse.x, mouse.y);
                                edgeWindowManager.endRepositioning(pt.x, pt.y);
                            }
                            isDragging = false;
                            dragArmed = false;
                        }

                        onCanceled: () => {
                            if (isDragging) {
                                var pt = centerDragArea.mapToGlobal(0, 0);
                                edgeWindowManager.endRepositioning(pt.x, pt.y);
                            }
                            isDragging = false;
                            dragArmed = false;
                        }
                    }
                }

                // Right: 3 buttons (Pin, Settings, Quit) - 26x26dp, radius 7, 4dp spacing, 14dp icon
                Row {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 4

                    // Pin button
                    Rectangle {
                        width: 26
                        height: 26
                        radius: 7
                        color: settingsManager.isPinned ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14) : (pinHover.hovered ? Theme.surfaceHover : "transparent")
                        border.color: settingsManager.isPinned ? Theme.accent : "transparent"
                        border.width: settingsManager.isPinned ? 1 : 0

                        scale: pinBtnMouse.pressed ? 0.98 : 1.0
                        Behavior on scale { NumberAnimation { duration: 100 } }
                        Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                        HoverHandler {
                            id: pinHover
                            blocking: false
                        }

                        TintedIcon {
                            anchors.centerIn: parent
                            size: 14
                            source: "qrc:/qt/qml/EdgeDrop/resources/icons/pin.svg"
                            color: settingsManager.isPinned ? Theme.accent : (pinHover.hovered ? Theme.textPrimary : Theme.textSecondary)
                        }

                        AppToolTip {
                            visible: pinHover.hovered
                            text: settingsManager.isPinned ? "Unpin notch" : "Keep notch open"
                        }

                        MouseArea {
                            id: pinBtnMouse
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: settingsManager.isPinned = !settingsManager.isPinned
                        }
                    }

                    // Settings button
                    Rectangle {
                        width: 26
                        height: 26
                        radius: 7
                        color: mainWindow.settingsOpen ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.14) : (setHover.hovered ? Theme.surfaceHover : "transparent")
                        border.color: mainWindow.settingsOpen ? Theme.accent : "transparent"
                        border.width: mainWindow.settingsOpen ? 1 : 0

                        scale: setBtnMouse.pressed ? 0.98 : 1.0
                        Behavior on scale { NumberAnimation { duration: 100 } }
                        Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                        HoverHandler {
                            id: setHover
                            blocking: false
                        }

                        TintedIcon {
                            anchors.centerIn: parent
                            size: 14
                            source: "qrc:/qt/qml/EdgeDrop/resources/icons/settings.svg"
                            color: mainWindow.settingsOpen ? Theme.accent : (setHover.hovered ? Theme.textPrimary : Theme.textSecondary)
                        }

                        AppToolTip {
                            visible: setHover.hovered
                            text: "Settings"
                        }

                        MouseArea {
                            id: setBtnMouse
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                mainWindow.settingsOpen = !mainWindow.settingsOpen;
                            }
                        }
                    }

                    // Quit button
                    Rectangle {
                        width: 26
                        height: 26
                        radius: 7
                        color: quitHover.hovered ? Theme.surfaceHover : "transparent"

                        scale: quitBtnMouse.pressed ? 0.98 : 1.0
                        Behavior on scale { NumberAnimation { duration: 100 } }
                        Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

                        HoverHandler {
                            id: quitHover
                            blocking: false
                        }

                        TintedIcon {
                            anchors.centerIn: parent
                            size: 14
                            source: "qrc:/qt/qml/EdgeDrop/resources/icons/quit.svg"
                            color: quitHover.hovered ? Theme.textPrimary : Theme.textSecondary
                        }

                        AppToolTip {
                            visible: quitHover.hovered
                            text: "Quit EdgeDrop"
                        }

                        MouseArea {
                            id: quitBtnMouse
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: Qt.quit()
                        }
                    }
                }
            }

            property string lastErrorMessage: ""
            property bool showErrorBanner: false

            Timer {
                id: errorBannerTimer
                interval: 4000
                repeat: false
                onTriggered: expandedContent.showErrorBanner = false
            }

            // Inline error banner beneath topBar
            Rectangle {
                id: errorBanner
                anchors.top: topBar.bottom
                anchors.topMargin: expandedContent.showErrorBanner ? 6 : 0
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.right: parent.right
                anchors.rightMargin: 16
                height: expandedContent.showErrorBanner ? (errorText.implicitHeight + 12) : 0
                visible: height > 0
                opacity: expandedContent.showErrorBanner ? 1.0 : 0.0
                radius: Theme.radiusSmall
                color: Qt.rgba(Theme.error.r, Theme.error.g, Theme.error.b, 0.15)
                border.color: Theme.error
                border.width: 1
                clip: true
                Behavior on height { NumberAnimation { duration: 150; easing.type: Easing.OutCubic } }
                Behavior on opacity { NumberAnimation { duration: 150 } }

                Row {
                    anchors.centerIn: parent
                    width: parent.width - 16
                    spacing: 6

                    Text {
                        text: "!"
                        font.pixelSize: 11
                        font.weight: Font.Bold
                        color: Theme.error
                        anchors.verticalCenter: parent.verticalCenter
                    }

                    Text {
                        id: errorText
                        width: parent.width - 18
                        text: expandedContent.lastErrorMessage
                        font.pixelSize: 11
                        color: Theme.error
                        wrapMode: Text.Wrap
                        elide: Text.ElideRight
                        maximumLineCount: 2
                        anchors.verticalCenter: parent.verticalCenter
                    }
                }
            }

            // ── Content Container (Strictly below errorBanner, 16dp margins) ───────────
            Item {
                id: viewsContainer
                anchors.top: errorBanner.bottom
                anchors.topMargin: expandedContent.showErrorBanner ? 6 : 12
                anchors.left: parent.left
                anchors.leftMargin: 16
                anchors.right: parent.right
                anchors.rightMargin: 16
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 16

                DropZoneCard {
                    id: dropCard
                    anchors.fill: parent
                    visible: mainWindow.activeView === "home"
                    enabled: visible
                    opacity: visible ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 160 } }
                }

                ConfigPanel {
                    id: configPanel
                    anchors.fill: parent
                    visible: mainWindow.activeView === "config"
                    enabled: visible
                    opacity: visible ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 160 } }
                }

                ResultCard {
                    id: resultCard
                    anchors.fill: parent
                    visible: mainWindow.activeView === "result"
                    enabled: visible
                    opacity: visible ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 160 } }
                }

                SettingsPopover {
                    id: settingsPopover
                    anchors.fill: parent
                    visible: mainWindow.activeView === "settings"
                    enabled: visible
                    opacity: visible ? 1.0 : 0.0
                    Behavior on opacity { NumberAnimation { duration: 160 } }
                    onClosed: {
                        mainWindow.settingsOpen = false;
                    }
                }
            }
        }

        // ── Production-Grade Resize Handles ───────────────────────────────────────
        // 1. Left Edge Drag Zone (~6px invisible)
        MouseArea {
            id: resizeLeftEdge
            visible: edgeWindowManager.isExpanded && !edgeWindowManager.isRepositioning && !edgeWindowManager.isSnapping
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 16
            anchors.bottomMargin: 16
            width: 6
            z: 998
            hoverEnabled: true
            preventStealing: true
            cursorShape: Qt.SizeHorCursor
            onPressed: edgeWindowManager.startCardResize(edgeWindowManager.handleLeft)
            onPositionChanged: edgeWindowManager.updateCardResize()
            onReleased: edgeWindowManager.endCardResize()
        }

        // 2. Right Edge Drag Zone (~6px invisible)
        MouseArea {
            id: resizeRightEdge
            visible: edgeWindowManager.isExpanded && !edgeWindowManager.isRepositioning && !edgeWindowManager.isSnapping
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            anchors.topMargin: 16
            anchors.bottomMargin: 16
            width: 6
            z: 998
            hoverEnabled: true
            preventStealing: true
            cursorShape: Qt.SizeHorCursor
            onPressed: edgeWindowManager.startCardResize(edgeWindowManager.handleRight)
            onPositionChanged: edgeWindowManager.updateCardResize()
            onReleased: edgeWindowManager.endCardResize()
        }

        // 3. Bottom Edge Drag Zone (~6px invisible)
        MouseArea {
            id: resizeBottomEdge
            visible: edgeWindowManager.isExpanded && !edgeWindowManager.isRepositioning && !edgeWindowManager.isSnapping
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.leftMargin: 16
            anchors.rightMargin: 16
            height: 6
            z: 998
            hoverEnabled: true
            preventStealing: true
            cursorShape: Qt.SizeVerCursor
            onPressed: edgeWindowManager.startCardResize(edgeWindowManager.handleBottom)
            onPositionChanged: edgeWindowManager.updateCardResize()
            onReleased: edgeWindowManager.endCardResize()
        }

        // 4. Bottom-Right Corner Resize Grip (Subtle, recessed corner indicator)
        Item {
            id: resizeBottomRightCorner
            visible: edgeWindowManager.isExpanded && !edgeWindowManager.isRepositioning && !edgeWindowManager.isSnapping
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            anchors.bottomMargin: 3
            anchors.rightMargin: 3
            width: 12
            height: 12
            z: 999

            Canvas {
                id: resizeCornerCanvas
                anchors.fill: parent
                opacity: (resizeCornerMouse.containsMouse || resizeCornerMouse.pressed) ? 0.85 : 0.25
                Behavior on opacity { NumberAnimation { duration: 150 } }

                onPaint: {
                    var ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    ctx.strokeStyle = (resizeCornerMouse.containsMouse || resizeCornerMouse.pressed) ? Theme.accent : Theme.textMuted;
                    ctx.lineWidth = 1.2;
                    ctx.lineCap = "round";
                    ctx.beginPath();
                    // Subtle curved corner arc 1
                    ctx.moveTo(width - 2, height - 7);
                    ctx.lineTo(width - 7, height - 2);
                    // Subtle curved corner arc 2
                    ctx.moveTo(width - 2, height - 4);
                    ctx.lineTo(width - 4, height - 2);
                    ctx.stroke();
                }
            }

            MouseArea {
                id: resizeCornerMouse
                anchors.fill: parent
                hoverEnabled: true
                preventStealing: true
                cursorShape: Qt.SizeFDiagCursor
                onPressed: edgeWindowManager.startCardResize(edgeWindowManager.handleBottomRight)
                onPositionChanged: edgeWindowManager.updateCardResize()
                onReleased: edgeWindowManager.endCardResize()
                onDoubleClicked: edgeWindowManager.resetCustomSize()
                onContainsMouseChanged: resizeCornerCanvas.requestPaint()
            }
        }

        // 5. Bottom-Left Corner Hit Area (Invisible hit area when docked on right edge)
        MouseArea {
            id: resizeBottomLeftMouse
            visible: edgeWindowManager.isExpanded && !edgeWindowManager.isRepositioning && !edgeWindowManager.isSnapping && edgeWindowManager.screenEdge === "right"
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.bottomMargin: 2
            anchors.leftMargin: 2
            width: 12
            height: 12
            z: 999
            hoverEnabled: true
            preventStealing: true
            cursorShape: Qt.SizeBDiagCursor
            onPressed: edgeWindowManager.startCardResize(edgeWindowManager.handleBottomLeft)
            onPositionChanged: edgeWindowManager.updateCardResize()
            onReleased: edgeWindowManager.endCardResize()
            onDoubleClicked: edgeWindowManager.resetCustomSize()
        }
    }
}
