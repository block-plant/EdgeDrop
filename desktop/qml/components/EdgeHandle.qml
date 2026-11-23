import QtQuick
import QtQuick.Controls
import EdgeDrop

Item {
    id: root
    width: 14
    height: 68

    signal triggered()

    readonly property bool isRightEdge: edgeWindowManager.screenEdge !== "left"

    HoverHandler {
        id: handleHover
        blocking: false
    }

    // Sleek rounded pill strictly locked to 10dp x 56dp, radius: 5
    // Centered in parent (14x68 window) with symmetric 2dp side insets and 6dp top/bottom insets
    Rectangle {
        id: visiblePill
        width: 10
        height: 56
        radius: width / 2

        anchors.centerIn: parent

        color: (dropZoneArea.containsDrag || (typeof rootDropArea !== "undefined" && rootDropArea.containsDrag)) ? Theme.accent : (handleHover.hovered ? Theme.elevatedHover : Theme.pillSurface)
        opacity: 1.0

        border.width: 1
        border.color: (dropZoneArea.containsDrag || (typeof rootDropArea !== "undefined" && rootDropArea.containsDrag)) ? Theme.accent : Theme.borderHairline

        scale: edgeWindowManager.isRepositioning ? 1.04 : (mouseArea.pressed ? 0.98 : 1.0)
        Behavior on scale { NumberAnimation { duration: 100 } }
        Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }

        // Subtle translucent grab indicator (3x20dp, radius: 1.5)
        Rectangle {
            id: grabIndicator
            width: 3
            height: 20
            radius: 1.5
            anchors.centerIn: parent
            z: 10
            visible: true
            opacity: 1.0
            color: Theme.isDarkMode ? Qt.rgba(1.0, 1.0, 1.0, 0.28) : Qt.rgba(0.0, 0.0, 0.0, 0.22)
        }
    }

    // Hover timer: 80ms delay prevents accidental wake
    Timer {
        id: hoverTimer
        interval: 80
        repeat: false
        onTriggered: {
            if (handleHover.hovered && !edgeWindowManager.isRepositioning && !edgeWindowManager.isSnapping) {
                root.triggered();
            }
        }
    }

    Connections {
        target: handleHover
        function onHoveredChanged() {
            if (handleHover.hovered && !edgeWindowManager.isRepositioning && !edgeWindowManager.isSnapping) {
                hoverTimer.restart();
            } else {
                hoverTimer.stop();
            }
        }
    }

    MouseArea {
        id: mouseArea
        anchors.fill: parent
        hoverEnabled: false
        acceptedButtons: Qt.LeftButton
        cursorShape: edgeWindowManager.isRepositioning ? Qt.ClosedHandCursor : Qt.PointingHandCursor

        property bool isDragging: false
        property bool dragArmed: false
        property int pressGlobalX: 0
        property int pressGlobalY: 0

        onPressed: (mouse) => {
            if (mouse.button !== Qt.LeftButton) return;
            hoverTimer.stop();
            isDragging = false;
            dragArmed = true;
            var pt = mouseArea.mapToGlobal(mouse.x, mouse.y);
            pressGlobalX = pt.x;
            pressGlobalY = pt.y;
        }

        onPositionChanged: (mouse) => {
            if (!dragArmed) return;

            // Anti-stick guard: abort immediately if button released outside
            if ((mouse.buttons & Qt.LeftButton) === 0) {
                if (isDragging) {
                    var endPt = mouseArea.mapToGlobal(mouse.x, mouse.y);
                    edgeWindowManager.endRepositioning(endPt.x, endPt.y);
                }
                isDragging = false;
                dragArmed = false;
                return;
            }

            var pt = mouseArea.mapToGlobal(mouse.x, mouse.y);
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
            var pt = mouseArea.mapToGlobal(mouse.x, mouse.y);
            if (isDragging) {
                edgeWindowManager.endRepositioning(pt.x, pt.y);
            } else if (dragArmed) {
                root.triggered();
            }
            isDragging = false;
            dragArmed = false;
        }

        onCanceled: () => {
            if (isDragging) {
                var pt = mouseArea.mapToGlobal(0, 0);
                edgeWindowManager.endRepositioning(pt.x, pt.y);
            }
            isDragging = false;
            dragArmed = false;
        }
    }

    DropArea {
        id: dropZoneArea
        anchors.fill: parent
        onEntered: (drag) => {
            if (!edgeWindowManager.isRepositioning && !edgeWindowManager.isSnapping) {
                root.triggered();
            }
        }
        onDropped: (drop) => {
            if (drop.hasUrls) {
                appController.loadFiles(drop.urls);
                drop.acceptProposedAction();
            }
        }
    }
}
