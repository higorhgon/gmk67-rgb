import QtQuick

// The GMK67 drawn from the same key map the frames are sent with, each key
// lit with what the keyboard shows: animation frames as they go out, or the
// look of the firmware effect that's picked. While editing, it is the
// remap editor instead: keys can be picked, and a remapped key shows its new
// function with its own legend in the corner.
Item {
    id: preview

    property bool editing: false
    property int selected: -1
    signal keyPicked(int index)

    readonly property var ui: theme.colors
    readonly property real unit: width / 16
    readonly property real gap: Math.max(2, unit * 0.08)

    implicitHeight: unit * 5

    // The knob, top right, where a key would be; it has no light.
    Rectangle {
        x: 15 * preview.unit + preview.gap
        y: preview.gap
        width: preview.unit - 2 * preview.gap
        height: width
        radius: width / 2
        color: preview.ui.button
        border.color: preview.ui.border
        Rectangle {
            anchors.centerIn: parent
            width: parent.width * 0.62
            height: width
            radius: width / 2
            color: "transparent"
            border.color: preview.ui.border
            border.width: 2
        }
    }

    Repeater {
        model: backend.keys
        delegate: Rectangle {
            id: key
            required property var modelData
            required property int index
            readonly property color lit: backend.preview[index] ?? "black"
            // How bright the light is, to keep the legend readable on it.
            readonly property real level: preview.editing ? 0 : Math.max(lit.r, lit.g, lit.b)
            readonly property int remap: preview.editing ? (backend.remaps[index] ?? 0) : 0
            readonly property bool usable: !preview.editing || backend.remappable(index)
            readonly property bool picked: preview.editing && preview.selected === index

            x: (modelData.x - modelData.width / 2) * preview.unit + preview.gap
            y: modelData.y * preview.unit + preview.gap
            width: modelData.width * preview.unit - 2 * preview.gap
            height: preview.unit - 2 * preview.gap
            radius: Math.min(4, theme.radius)
            opacity: usable ? 1 : 0.4
            // A dark key cap with the light showing through it; an unlit key
            // keeps the theme's button color.
            color: picked ? Qt.tint(preview.ui.button, Qt.alpha(preview.ui.accent, 0.45))
                 : remap ? Qt.tint(preview.ui.button, Qt.alpha(preview.ui.accent, 0.18))
                 : level < 0.02 ? preview.ui.button : Qt.tint(preview.ui.button, Qt.alpha(lit, 0.25 + 0.75 * level))
            border.color: picked ? preview.ui.foreground
                        : remap ? preview.ui.accent
                        : level < 0.02 ? preview.ui.border : Qt.alpha(lit, 0.9)
            border.width: picked ? 2 : 1
            Behavior on color { enabled: !backend.animating || preview.editing; ColorAnimation { duration: 140 } }

            Text {
                anchors.centerIn: parent
                width: parent.width - 4
                horizontalAlignment: Text.AlignHCenter
                elide: Text.ElideRight
                text: key.remap ? backend.functionLabel(key.remap) : key.modelData.label
                font.pixelSize: Math.max(8, Math.round(preview.unit * 0.22))
                font.weight: key.remap ? Font.DemiBold : Font.Normal
                color: key.level > 0.55 ? Qt.rgba(0, 0, 0, 0.7) : key.remap || key.picked ? preview.ui.foreground : preview.ui.muted
            }
            // The key's own legend, small, once it does something else.
            Text {
                visible: key.remap !== 0
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: 3
                text: key.modelData.label
                font.pixelSize: Math.max(7, Math.round(preview.unit * 0.15))
                color: preview.ui.muted
            }
            HoverHandler { enabled: preview.editing && key.usable; cursorShape: Qt.PointingHandCursor }
            TapHandler {
                enabled: preview.editing && key.usable
                onTapped: preview.keyPicked(key.index)
            }
        }
    }
}
