import QtQuick
import QtQuick.Controls

// A slider in the same look as the other controls: a thin track filled with
// the accent up to a round handle.
Slider {
    id: control

    readonly property var ui: theme.colors

    implicitHeight: 28
    stepSize: 1
    snapMode: Slider.SnapAlways
    focusPolicy: Qt.TabFocus

    background: Rectangle {
        x: control.leftPadding
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: control.availableWidth
        height: 4
        radius: 2
        color: control.ui.border
        Rectangle {
            width: control.visualPosition * parent.width
            height: parent.height
            radius: 2
            color: control.ui.accent
        }
    }

    handle: Rectangle {
        x: control.leftPadding + control.visualPosition * (control.availableWidth - width)
        y: control.topPadding + control.availableHeight / 2 - height / 2
        width: 16
        height: 16
        radius: 8
        color: control.pressed || control.hovered ? control.ui.accentHover : control.ui.accent
        border.color: control.visualFocus ? control.ui.foreground : "transparent"
        border.width: 2
    }
}
