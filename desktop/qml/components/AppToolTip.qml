import QtQuick
import QtQuick.Controls
import EdgeDrop

ToolTip {
    id: control
    delay: 350
    timeout: 5000
    topPadding: 6
    bottomPadding: 6
    leftPadding: 8
    rightPadding: 8
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutsideParent
    enabled: false

    // Position clean 8dp offset away so cursor never touches tooltip box
    y: parent ? (parent.height + 8) : 8

    enter: Transition {
        NumberAnimation { property: "opacity"; from: 0.0; to: 1.0; duration: Theme.animHoverMs; easing.type: Easing.OutCubic }
    }
    exit: Transition {
        NumberAnimation { property: "opacity"; from: 1.0; to: 0.0; duration: 100; easing.type: Easing.OutCubic }
    }

    contentItem: Text {
        enabled: false
        text: control.text
        font.pixelSize: 11
        font.weight: Font.Normal
        color: Theme.textPrimary
    }

    background: Rectangle {
        enabled: false
        radius: 7
        color: Theme.surfaceElevated
        border.color: Theme.borderHairline
        border.width: 1
    }
}
