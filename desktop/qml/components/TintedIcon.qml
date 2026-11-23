import QtQuick
import QtQuick.Effects
import EdgeDrop

Item {
    id: root

    property string source: ""
    property color color: Theme.primaryText
    Behavior on color { ColorAnimation { duration: Theme.animHoverMs; easing.type: Easing.OutCubic } }
    property real size: 16
    property real iconWidth: size
    property real iconHeight: size

    implicitWidth: iconWidth
    implicitHeight: iconHeight

    Image {
        id: srcImg
        anchors.fill: parent
        source: root.source
        sourceSize.width: Math.round(root.iconWidth * (Screen.devicePixelRatio || 2))
        sourceSize.height: Math.round(root.iconHeight * (Screen.devicePixelRatio || 2))
        fillMode: Image.PreserveAspectFit
        smooth: true
        mipmap: true
        visible: false
    }

    MultiEffect {
        anchors.fill: srcImg
        source: srcImg
        colorization: 1.0
        colorizationColor: root.color
        brightness: 1.0
    }
}
