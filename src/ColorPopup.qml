import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A compact color picker: saturation and value in a square, the hue in a
// bar under it, and the hex code to type one in.
Popup {
    id: popup

    property color color: "white"
    readonly property var ui: theme.colors
    property real hue: 0
    property real saturation: 0
    property real value: 1
    property bool syncing: false

    signal picked(color color)

    function load(c) {
        syncing = true
        // A gray has no hue: keep the last one, so the bar doesn't jump.
        if (c.hsvSaturation > 0)
            hue = Math.max(c.hsvHue, 0)
        saturation = c.hsvSaturation
        value = c.hsvValue
        hex.text = c.toString()
        syncing = false
    }

    function update() {
        const c = Qt.hsva(hue, saturation, value, 1)
        hex.text = c.toString()
        if (!syncing)
            picked(c)
    }

    onAboutToShow: load(color)
    padding: 12
    modal: false
    focus: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    background: Rectangle {
        color: popup.ui.panel
        border.color: popup.ui.border
        radius: theme.radius
    }

    contentItem: ColumnLayout {
        spacing: 10

        Rectangle {
            id: square
            Layout.preferredWidth: 240
            Layout.preferredHeight: 150
            radius: Math.min(4, theme.radius)
            color: Qt.hsva(popup.hue, 1, 1, 1)
            clip: true
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0; color: "white" }
                    GradientStop { position: 1; color: "transparent" }
                }
            }
            Rectangle {
                anchors.fill: parent
                radius: parent.radius
                gradient: Gradient {
                    GradientStop { position: 0; color: "transparent" }
                    GradientStop { position: 1; color: "black" }
                }
            }
            Rectangle {
                x: popup.saturation * square.width - width / 2
                y: (1 - popup.value) * square.height - height / 2
                width: 14
                height: 14
                radius: 7
                color: "transparent"
                border.color: "white"
                border.width: 2
            }
            MouseArea {
                anchors.fill: parent
                preventStealing: true
                function pick(mouse) {
                    popup.saturation = Math.min(Math.max(mouse.x / width, 0), 1)
                    popup.value = 1 - Math.min(Math.max(mouse.y / height, 0), 1)
                    popup.update()
                }
                onPressed: mouse => pick(mouse)
                onPositionChanged: mouse => pick(mouse)
            }
        }

        Rectangle {
            id: bar
            Layout.preferredWidth: 240
            Layout.preferredHeight: 12
            radius: 6
            gradient: Gradient {
                orientation: Gradient.Horizontal
                GradientStop { position: 0.0; color: "#ff0000" }
                GradientStop { position: 0.17; color: "#ffff00" }
                GradientStop { position: 0.33; color: "#00ff00" }
                GradientStop { position: 0.5; color: "#00ffff" }
                GradientStop { position: 0.67; color: "#0000ff" }
                GradientStop { position: 0.83; color: "#ff00ff" }
                GradientStop { position: 1.0; color: "#ff0000" }
            }
            Rectangle {
                x: popup.hue * bar.width - width / 2
                anchors.verticalCenter: parent.verticalCenter
                width: 6
                height: 18
                radius: 3
                color: "white"
                border.color: Qt.rgba(0, 0, 0, 0.4)
            }
            MouseArea {
                anchors.fill: parent
                anchors.margins: -6
                preventStealing: true
                function pick(mouse) {
                    popup.hue = Math.min(Math.max((mouse.x - 6) / bar.width, 0), 0.999)
                    popup.update()
                }
                onPressed: mouse => pick(mouse)
                onPositionChanged: mouse => pick(mouse)
            }
        }

        AppTextField {
            id: hex
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: 110
            horizontalAlignment: Text.AlignHCenter
            font.family: "monospace"
            maximumLength: 7
            onAccepted: {
                if (/^#?[0-9a-fA-F]{6}$/.test(text)) {
                    // Qt.tint with a transparent tint turns a string into a color.
                    const c = Qt.tint(text.startsWith("#") ? text : "#" + text, "transparent")
                    popup.load(c)
                    popup.picked(c)
                } else {
                    popup.load(popup.color)
                }
            }
        }
    }
}
