import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

ApplicationWindow {
    id: win

    readonly property var ui: theme.colors
    readonly property var info: backend.modeInfo
    readonly property bool overlayOpen: shortcuts.opened || colorPopup.opened
    // "lighting", "keys" or "macros", the parts of the window.
    property string page: "lighting"
    readonly property var pages: ["lighting", "keys", "macros"]
    // The macro being edited, a slot.
    property int pickedMacro: -1
    readonly property var macro: pickedMacro >= 0 && pickedMacro < backend.macros.length ? backend.macros[pickedMacro] : null
    readonly property bool lighting: page === "lighting"
    // A text field has the keyboard; single-key shortcuts stay out of it.
    readonly property bool typing: activeFocusItem instanceof TextInput
    // The key picked for remapping, a Protocol::keys() index.
    property int pickedKey: -1
    readonly property int pickedRemap: pickedKey >= 0 ? (backend.remaps[pickedKey] ?? 0) : 0

    width: 1000
    height: 720
    minimumWidth: 760
    minimumHeight: 600
    visible: true
    title: "GMK67"
    color: ui.background

    function step(delta) {
        const list = backend.modes
        const i = list.findIndex(m => m.value === backend.mode)
        backend.mode = list[Math.min(Math.max(i + delta, 0), list.length - 1)].value
    }

    function statusText() {
        switch (backend.status) {
        case 0: return backend.animating ? qsTr("Connected, animating") : qsTr("Connected by USB")
        case 1: return qsTr("Not connected")
        case 2: return qsTr("No permission")
        default: return qsTr("Not responding")
        }
    }

    // The bar's main button: apply the effect, or save the keys.
    function mainAction() {
        if (win.lighting)
            backend.apply()
        else if (backend.keymapDirty)
            backend.saveKeymap()
    }

    function activeName() {
        const m = backend.modes.find(m => m.value === backend.activeMode)
        return m ? m.name : ""
    }

    Shortcut { sequences: ["?", "F1"]; context: Qt.ApplicationShortcut; onActivated: shortcuts.visible ? shortcuts.close() : shortcuts.open() }
    Shortcut { sequences: ["Q", "Ctrl+Q"]; enabled: !win.overlayOpen && !win.typing; context: Qt.ApplicationShortcut; onActivated: Qt.quit() }
    Shortcut { sequences: ["Space", "Return", "Enter"]; enabled: !win.overlayOpen && !win.typing; context: Qt.ApplicationShortcut; onActivated: win.mainAction() }
    Shortcut { sequence: "Esc"; enabled: !win.overlayOpen && win.lighting && backend.animating; context: Qt.ApplicationShortcut; onActivated: backend.stopAnimation() }
    Shortcut { sequence: "Esc"; enabled: !win.overlayOpen && win.page === "keys" && win.pickedKey >= 0; context: Qt.ApplicationShortcut; onActivated: win.pickedKey = -1 }
    Shortcut { sequence: "Tab"; enabled: !win.overlayOpen && !win.typing; context: Qt.ApplicationShortcut; onActivated: win.page = win.pages[(win.pages.indexOf(win.page) + 1) % win.pages.length] }
    Shortcut { sequences: ["Delete", "Backspace"]; enabled: !win.overlayOpen && !win.typing && win.page === "keys" && win.pickedKey >= 0; context: Qt.ApplicationShortcut; onActivated: backend.resetKey(win.pickedKey) }
    Shortcut { sequences: ["Up", "K"]; enabled: !win.overlayOpen && win.lighting; context: Qt.ApplicationShortcut; onActivated: win.step(-1) }
    Shortcut { sequences: ["Down", "J"]; enabled: !win.overlayOpen && win.lighting; context: Qt.ApplicationShortcut; onActivated: win.step(1) }
    Shortcut { sequence: "Left"; enabled: !win.overlayOpen && win.lighting; context: Qt.ApplicationShortcut; onActivated: backend.brightness -= 1 }
    Shortcut { sequence: "Right"; enabled: !win.overlayOpen && win.lighting; context: Qt.ApplicationShortcut; onActivated: backend.brightness += 1 }
    Shortcut { sequence: "Shift+Left"; enabled: !win.overlayOpen && win.lighting && win.info.speed === true; context: Qt.ApplicationShortcut; onActivated: backend.speed -= 1 }
    Shortcut { sequence: "Shift+Right"; enabled: !win.overlayOpen && win.lighting && win.info.speed === true; context: Qt.ApplicationShortcut; onActivated: backend.speed += 1 }
    Shortcut { sequence: "R"; enabled: !win.overlayOpen && win.lighting && win.info.random === true; context: Qt.ApplicationShortcut; onActivated: backend.random = !backend.random }
    Shortcut { sequence: "C"; enabled: !win.overlayOpen && win.lighting && win.info.color === true; context: Qt.ApplicationShortcut; onActivated: colorPopup.open() }
    Repeater {
        model: 8
        Item {
            required property int index
            Shortcut {
                sequence: String(index + 1)
                enabled: !win.overlayOpen && win.lighting && win.info.color === true
                context: Qt.ApplicationShortcut
                onActivated: backend.color = backend.recentColors[index]
            }
        }
    }

    ShortcutsOverlay { id: shortcuts }

    // Right-to-left languages mirror the window; the Translator sets the
    // application's layout direction.
    Item {
        anchors.fill: parent
        LayoutMirroring.enabled: Qt.application.layoutDirection === Qt.RightToLeft
        LayoutMirroring.childrenInherit: true

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            // The bar: the keyboard's state, help, and the one main action.
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 56
                color: win.ui.panel
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: win.ui.border }

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 18
                    anchors.rightMargin: 14
                    spacing: 12

                    AppIcon { name: "keyboard"; color: win.ui.accent; Layout.preferredWidth: 22; Layout.preferredHeight: 22 }
                    Label {
                        text: "GMK67"
                        font.pixelSize: 16
                        font.weight: Font.DemiBold
                        color: win.ui.foreground
                    }
                    Rectangle {
                        Layout.leftMargin: 6
                        implicitWidth: statusRow.implicitWidth + 20
                        implicitHeight: 26
                        radius: Math.min(13, theme.radius * 3)
                        color: win.ui.background
                        border.color: win.ui.border
                        RowLayout {
                            id: statusRow
                            anchors.centerIn: parent
                            spacing: 7
                            Rectangle {
                                implicitWidth: 8; implicitHeight: 8; radius: 4
                                color: backend.status === 0 ? win.ui.green : backend.status === 1 ? win.ui.muted : win.ui.error
                            }
                            Label { text: win.statusText(); font.pixelSize: 12; color: win.ui.muted }
                        }
                    }
                    Item { Layout.fillWidth: true }
                    AppSegmented {
                        implicitWidth: 330
                        options: [{ label: qsTr("Lighting"), value: "lighting" }, { label: qsTr("Keys"), value: "keys" },
                                  { label: qsTr("Macros"), value: "macros" }]
                        value: win.page
                        onPicked: v => win.page = v
                    }
                    Item { Layout.fillWidth: true }
                    IconButton {
                        iconName: "help"
                        description: qsTr("Keyboard shortcuts (?)")
                        onClicked: shortcuts.open()
                    }
                    IconButton {
                        visible: backend.animating
                        iconName: "stop"
                        description: qsTr("Stop the animation (Esc)")
                        onClicked: backend.stopAnimation()
                    }
                    AppButton {
                        primary: true
                        enabled: !backend.busy && (win.lighting || backend.keymapDirty)
                        text: !win.lighting ? qsTr("Save to keyboard") : win.info.animated ? qsTr("Play") : qsTr("Apply")
                        onClicked: win.mainAction()
                    }
                }
            }

            // What's wrong, said in the window, with what fixes it.
            Rectangle {
                Layout.fillWidth: true
                visible: backend.status !== 0
                implicitHeight: problem.implicitHeight + 24
                color: Qt.tint(win.ui.panel, Qt.alpha(backend.status === 1 ? win.ui.accent : win.ui.error, 0.10))
                Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: win.ui.border }

                ColumnLayout {
                    id: problem
                    anchors.fill: parent
                    anchors.margins: 12
                    anchors.leftMargin: 18
                    anchors.rightMargin: 18
                    spacing: 10
                    RowLayout {
                        spacing: 10
                        AppIcon {
                            name: backend.status === 2 ? "lock" : "plug"
                            color: backend.status === 1 ? win.ui.accent : win.ui.error
                            Layout.preferredWidth: 18; Layout.preferredHeight: 18
                        }
                        Label {
                            Layout.fillWidth: true
                            text: backend.statusMessage
                            wrapMode: Text.Wrap
                            font.pixelSize: 13
                            color: win.ui.foreground
                        }
                        AppButton {
                            visible: backend.status !== 2
                            flat: true
                            text: qsTr("Try again")
                            onClicked: backend.refresh()
                        }
                    }
                    RowLayout {
                        visible: backend.status === 2
                        spacing: 10
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: udev.implicitHeight + 16
                            radius: Math.min(4, theme.radius)
                            color: win.ui.background
                            border.color: win.ui.border
                            Label {
                                id: udev
                                anchors.fill: parent
                                anchors.margins: 8
                                text: backend.udevCommand
                                font.family: "monospace"
                                font.pixelSize: 11
                                wrapMode: Text.WrapAnywhere
                                color: win.ui.foreground
                            }
                        }
                        AppButton { text: qsTr("Copy"); onClicked: backend.copyUdevCommand() }
                    }
                }
            }

            // The keyboard, lit as it is.
            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: keyboardView.implicitHeight + 64
                KeyboardPreview {
                    id: keyboardView
                    editing: !win.lighting
                    selected: win.pickedKey
                    onKeyPicked: i => { win.pickedKey = i; win.page = "keys" }
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: parent.top
                    anchors.topMargin: 22
                    width: Math.min(parent.width - 48, 760)
                }
                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    anchors.top: keyboardView.bottom
                    anchors.topMargin: 10
                    font.pixelSize: 12
                    color: win.ui.muted
                    text: !win.lighting ? (backend.keymapDirty ? qsTr("Changes not saved yet") : qsTr("Click a key, then pick what it should do"))
                        : backend.animating ? qsTr("Playing %1").arg(win.activeName())
                        : win.activeName() !== "" ? qsTr("On the keyboard: %1").arg(win.activeName())
                        : ""
                }
            }

            Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: win.ui.border }

            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: win.lighting
                spacing: 0

                // Every effect, the keyboard's own first, then the animated ones.
                ListView {
                    id: effects
                    Layout.preferredWidth: 270
                    Layout.fillHeight: true
                    clip: true
                    model: backend.modes
                    currentIndex: backend.modes.findIndex(m => m.value === backend.mode)
                    highlightFollowsCurrentItem: false
                    onCurrentIndexChanged: positionViewAtIndex(currentIndex, ListView.Contain)
                    Component.onCompleted: positionViewAtIndex(currentIndex, ListView.Center)
                    boundsBehavior: Flickable.StopAtBounds
                    topMargin: 8
                    bottomMargin: 8
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    section.property: "animated"
                    section.delegate: Label {
                        required property string section
                        leftPadding: 20
                        topPadding: 12
                        bottomPadding: 6
                        text: (section === "true" ? qsTr("Animated by this app") : qsTr("Keyboard effects")).toUpperCase()
                        font.pixelSize: 11
                        font.letterSpacing: 1
                        color: win.ui.accent
                    }
                    // ListView pins delegates to x 0, so the row is a full-width
                    // item and the highlight sits inside it with margins.
                    delegate: Item {
                        id: row
                        required property var modelData
                        required property int index
                        readonly property bool picked: modelData.value === backend.mode
                        width: ListView.view.width
                        height: 34
                        Rectangle {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            radius: Math.min(4, theme.radius)
                            color: row.picked ? win.ui.selection : rowHover.hovered ? win.ui.hover : "transparent"
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 12
                                anchors.rightMargin: 12
                                spacing: 8
                                Label {
                                    Layout.fillWidth: true
                                    text: row.modelData.name
                                    elide: Text.ElideRight
                                    font.pixelSize: 13
                                    font.weight: row.picked ? Font.DemiBold : Font.Normal
                                    color: row.picked ? win.ui.selectionText : win.ui.foreground
                                }
                                // The effect that's on the keyboard now.
                                Rectangle {
                                    visible: row.modelData.value === backend.activeMode
                                    implicitWidth: 7; implicitHeight: 7; radius: 3.5
                                    color: row.picked ? win.ui.selectionText : win.ui.accent
                                }
                            }
                        }
                        HoverHandler { id: rowHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler {
                            onTapped: backend.mode = row.modelData.value
                            onDoubleTapped: { backend.mode = row.modelData.value; backend.apply() }
                        }
                    }
                }

                Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: win.ui.border }

                // How the picked effect is set.
                Flickable {
                    id: settingsView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentHeight: settings.implicitHeight + 40
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    ColumnLayout {
                        id: settings
                        x: 28
                        y: 20
                        width: Math.min(settingsView.width - 56, 520)
                        spacing: 8

                        component Section: Label {
                            Layout.topMargin: 12
                            font.pixelSize: 11
                            font.letterSpacing: 1
                            color: win.ui.accent
                        }

                        Label {
                            text: win.info.name ?? ""
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                            color: win.ui.foreground
                        }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            font.pixelSize: 13
                            color: win.ui.muted
                            text: win.info.animated
                                  ? qsTr("Drawn here and sent to the keyboard live. It plays while this window is open; gmk67 animate keeps it going from a terminal.")
                                  : win.info.value === 0 ? qsTr("Turns the lights off.")
                                  : qsTr("Saved on the keyboard. It stays after you close the app or unplug the cable.")
                        }

                        Section { visible: win.info.color === true; text: qsTr("Color").toUpperCase() }
                        RowLayout {
                            visible: win.info.color === true
                            spacing: 8
                            Repeater {
                                model: backend.recentColors
                                Rectangle {
                                    id: swatch
                                    required property color modelData
                                    required property int index
                                    readonly property bool current: Qt.colorEqual(modelData, backend.color)
                                    implicitWidth: 30; implicitHeight: 30; radius: 15
                                    color: modelData
                                    border.width: current ? 2 : 1
                                    border.color: current ? win.ui.foreground : Qt.alpha(win.ui.foreground, 0.2)
                                    opacity: backend.random && win.info.random === true ? 0.4 : 1
                                    HoverHandler { id: swatchHover; cursorShape: Qt.PointingHandCursor }
                                    TapHandler { onTapped: backend.color = swatch.modelData }
                                    ToolTip.visible: swatchHover.hovered
                                    ToolTip.delay: 400
                                    ToolTip.text: modelData.toString() + "  (" + (index + 1) + ")"
                                }
                            }
                            Rectangle {
                                Layout.leftMargin: 6
                                implicitWidth: 46; implicitHeight: 30
                                radius: Math.min(4, theme.radius)
                                color: backend.color
                                border.color: Qt.alpha(win.ui.foreground, 0.25)
                                HoverHandler { cursorShape: Qt.PointingHandCursor }
                                TapHandler { onTapped: colorPopup.open() }
                                ColorPopup {
                                    id: colorPopup
                                    y: parent.height + 6
                                    color: backend.color
                                    onPicked: c => backend.color = c
                                }
                            }
                        }
                        AppCheckBox {
                            visible: win.info.random === true
                            text: qsTr("Random colors")
                            checked: backend.random
                            onToggled: backend.random = checked
                        }

                        Section { visible: win.info.brightness === true; text: qsTr("Brightness").toUpperCase() }
                        AppSlider {
                            visible: win.info.brightness === true
                            Layout.fillWidth: true
                            from: 0; to: backend.maxLevel
                            value: backend.brightness
                            onMoved: backend.brightness = value
                        }

                        Section { visible: win.info.speed === true; text: qsTr("Speed").toUpperCase() }
                        AppSlider {
                            visible: win.info.speed === true
                            Layout.fillWidth: true
                            from: 0; to: backend.maxLevel
                            value: backend.speed
                            onMoved: backend.speed = value
                        }

                        Section { visible: backend.directions.length > 0; text: qsTr("Direction").toUpperCase() }
                        AppSegmented {
                            visible: backend.directions.length > 0
                            Layout.fillWidth: true
                            options: backend.directions
                            value: backend.direction
                            onPicked: v => backend.direction = v
                        }
                    }
                }
            }
            // Remapping: what a key can do, and the picked key's state.
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: win.page === "keys"
                spacing: 0

                ListView {
                    id: functionsView
                    Layout.preferredWidth: 270
                    Layout.fillHeight: true
                    clip: true
                    model: backend.keyFunctions
                    enabled: win.pickedKey >= 0
                    opacity: enabled ? 1 : 0.5
                    // Brings the picked key's function into view.
                    Connections {
                        target: win
                        function onPickedKeyChanged() {
                            const i = backend.keyFunctions.findIndex(f => f.code === win.pickedRemap)
                            if (i >= 0)
                                functionsView.positionViewAtIndex(i, ListView.Contain)
                        }
                    }
                    boundsBehavior: Flickable.StopAtBounds
                    topMargin: 8
                    bottomMargin: 8
                    ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                    section.property: "group"
                    section.delegate: Label {
                        required property string section
                        leftPadding: 20
                        topPadding: 12
                        bottomPadding: 6
                        text: section.toUpperCase()
                        font.pixelSize: 11
                        font.letterSpacing: 1
                        color: win.ui.accent
                    }
                    delegate: Item {
                        id: fnRow
                        required property var modelData
                        readonly property bool picked: modelData.code === win.pickedRemap
                        width: ListView.view.width
                        height: 30
                        Rectangle {
                            anchors.fill: parent
                            anchors.leftMargin: 8
                            anchors.rightMargin: 8
                            radius: Math.min(4, theme.radius)
                            color: fnRow.picked ? win.ui.selection : fnHover.hovered ? win.ui.hover : "transparent"
                            Label {
                                anchors.verticalCenter: parent.verticalCenter
                                anchors.left: parent.left
                                anchors.leftMargin: 12
                                text: fnRow.modelData.name
                                font.pixelSize: 13
                                font.weight: fnRow.picked ? Font.DemiBold : Font.Normal
                                color: fnRow.picked ? win.ui.selectionText : win.ui.foreground
                            }
                        }
                        HoverHandler { id: fnHover; cursorShape: Qt.PointingHandCursor }
                        TapHandler { onTapped: backend.remapKey(win.pickedKey, fnRow.modelData.code) }
                    }
                }

                Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: win.ui.border }

                Flickable {
                    id: keyView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentHeight: keyPanel.implicitHeight + 40
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    ColumnLayout {
                        id: keyPanel
                        x: 28
                        y: 20
                        width: Math.min(keyView.width - 56, 520)
                        spacing: 8

                        Label {
                            text: win.pickedKey < 0 ? qsTr("Remap keys")
                                  : qsTr("Key %1").arg(backend.keys[win.pickedKey].label || qsTr("Space"))
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                            color: win.ui.foreground
                        }
                        Label {
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            font.pixelSize: 13
                            color: win.ui.muted
                            text: win.pickedKey < 0 ? qsTr("Click a key on the keyboard above, then pick what it should do from the list.")
                                  : win.pickedRemap ? qsTr("Does: %1").arg(backend.functionName(win.pickedRemap))
                                  : qsTr("Does what its legend says.")
                        }
                        AppButton {
                            visible: win.pickedKey >= 0
                            enabled: win.pickedRemap !== 0
                            text: qsTr("Restore this key")
                            onClicked: backend.resetKey(win.pickedKey)
                        }

                        Label {
                            Layout.topMargin: 12
                            font.pixelSize: 11
                            font.letterSpacing: 1
                            color: win.ui.accent
                            text: qsTr("All keys").toUpperCase()
                        }
                        Label {
                            font.pixelSize: 13
                            color: win.ui.foreground
                            text: qsTr("Changed keys: %1").arg(backend.remaps.filter(c => c !== 0).length)
                        }
                        RowLayout {
                            spacing: 8
                            AppButton {
                                enabled: backend.remaps.some(c => c !== 0)
                                text: qsTr("Restore all keys")
                                onClicked: backend.resetAllKeys()
                            }
                            AppButton {
                                flat: true
                                enabled: backend.keymapDirty
                                text: qsTr("Discard changes")
                                onClicked: backend.discardKeymap()
                            }
                        }
                        Label {
                            Layout.topMargin: 12
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            font.pixelSize: 12
                            color: win.ui.muted
                            text: qsTr("Saving writes the keys to the keyboard, where they stay. Fn and its layer keep their own functions. The keyboard can't report its keys, so this shows what this app saved last. If a remap makes typing hard, run gmk67 keys reset in a terminal.")
                        }
                    }
                }
            }
            // Macros: the list, and the picked one's steps.
            RowLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                visible: win.page === "macros"
                spacing: 0

                ColumnLayout {
                    Layout.preferredWidth: 270
                    Layout.maximumWidth: 270
                    Layout.fillHeight: true
                    spacing: 0
                    ListView {
                        id: macroList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        model: backend.macros
                        boundsBehavior: Flickable.StopAtBounds
                        topMargin: 8
                        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
                        delegate: Item {
                            id: macroRow
                            required property var modelData
                            required property int index
                            readonly property bool picked: index === win.pickedMacro
                            width: ListView.view.width
                            height: 34
                            Rectangle {
                                anchors.fill: parent
                                anchors.leftMargin: 8
                                anchors.rightMargin: 8
                                radius: Math.min(4, theme.radius)
                                color: macroRow.picked ? win.ui.selection : macroHover.hovered ? win.ui.hover : "transparent"
                                Label {
                                    anchors.verticalCenter: parent.verticalCenter
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.leftMargin: 12
                                    anchors.rightMargin: 12
                                    text: macroRow.modelData.title
                                    elide: Text.ElideRight
                                    font.pixelSize: 13
                                    font.weight: macroRow.picked ? Font.DemiBold : Font.Normal
                                    color: macroRow.picked ? win.ui.selectionText : win.ui.foreground
                                }
                            }
                            HoverHandler { id: macroHover; cursorShape: Qt.PointingHandCursor }
                            TapHandler { onTapped: win.pickedMacro = macroRow.index }
                        }
                    }
                    AppButton {
                        Layout.margins: 12
                        Layout.fillWidth: true
                        text: qsTr("New macro")
                        onClicked: win.pickedMacro = backend.addMacro()
                    }
                }

                Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: win.ui.border }

                Flickable {
                    id: macroView
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    contentHeight: macroPanel.implicitHeight + 40
                    clip: true
                    boundsBehavior: Flickable.StopAtBounds

                    ColumnLayout {
                        id: macroPanel
                        x: 28
                        y: 20
                        width: Math.min(macroView.width - 56, 560)
                        spacing: 8

                        Label {
                            visible: !win.macro
                            text: qsTr("Macros")
                            font.pixelSize: 20
                            font.weight: Font.DemiBold
                            color: win.ui.foreground
                        }
                        Label {
                            visible: !win.macro
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            font.pixelSize: 13
                            color: win.ui.muted
                            text: qsTr("A macro presses keys for you: combinations like Ctrl + Shift + T, or some text. Make one here, then give it to a key in the Keys tab.")
                        }

                        AppTextField {
                            visible: !!win.macro
                            Layout.fillWidth: true
                            placeholderText: qsTr("Name")
                            text: win.macro ? win.macro.name : ""
                            onEditingFinished: backend.renameMacro(win.pickedMacro, text)
                        }

                        Section { visible: !!win.macro; text: qsTr("Steps").toUpperCase() }
                        Label {
                            visible: !!win.macro && win.macro.steps.length === 0
                            font.pixelSize: 13
                            color: win.ui.muted
                            text: qsTr("No steps yet. Add a combination or some text below.")
                        }
                        Repeater {
                            model: win.macro ? win.macro.steps : []
                            delegate: RowLayout {
                                id: stepRow
                                required property string modelData
                                required property int index
                                Layout.fillWidth: true
                                spacing: 4
                                Label {
                                    Layout.fillWidth: true
                                    text: (stepRow.index + 1) + ".  " + stepRow.modelData
                                    elide: Text.ElideRight
                                    font.pixelSize: 13
                                    color: win.ui.foreground
                                }
                                IconButton {
                                    iconName: "up"
                                    description: qsTr("Earlier")
                                    enabled: stepRow.index > 0
                                    onClicked: backend.moveStep(win.pickedMacro, stepRow.index, -1)
                                }
                                IconButton {
                                    iconName: "down"
                                    description: qsTr("Later")
                                    enabled: stepRow.index < win.macro.steps.length - 1
                                    onClicked: backend.moveStep(win.pickedMacro, stepRow.index, 1)
                                }
                                IconButton {
                                    iconName: "close"
                                    description: qsTr("Remove the step")
                                    onClicked: backend.removeStep(win.pickedMacro, stepRow.index)
                                }
                            }
                        }

                        Section { visible: !!win.macro; text: qsTr("Add a combination").toUpperCase() }
                        RowLayout {
                            visible: !!win.macro
                            spacing: 10
                            AppCheckBox { id: modCtrl; text: "Ctrl" }
                            AppCheckBox { id: modShift; text: "Shift" }
                            AppCheckBox { id: modAlt; text: "Alt" }
                            AppCheckBox { id: modSuper; text: "Super" }
                        }
                        RowLayout {
                            visible: !!win.macro
                            Layout.fillWidth: true
                            spacing: 8
                            AppComboBox {
                                id: comboKey
                                Layout.fillWidth: true
                                textRole: "name"
                                model: backend.keyFunctions.filter(f => (f.code >> 16) === 2)
                            }
                            AppButton {
                                text: qsTr("Add")
                                onClicked: backend.addComboStep(win.pickedMacro,
                                    (modCtrl.checked ? 1 : 0) | (modShift.checked ? 2 : 0) | (modAlt.checked ? 4 : 0) | (modSuper.checked ? 8 : 0),
                                    comboKey.model[comboKey.currentIndex].code & 0xFFFF)
                            }
                        }

                        Section { visible: !!win.macro; text: qsTr("Add text").toUpperCase() }
                        RowLayout {
                            visible: !!win.macro
                            Layout.fillWidth: true
                            spacing: 8
                            AppTextField {
                                id: macroText
                                Layout.fillWidth: true
                                placeholderText: qsTr("Text to type")
                            }
                            AppButton {
                                text: qsTr("Add")
                                enabled: macroText.text !== "" && backend.typeable(macroText.text)
                                onClicked: if (backend.addTextStep(win.pickedMacro, macroText.text)) macroText.text = ""
                            }
                        }
                        Label {
                            visible: !!win.macro
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            font.pixelSize: 12
                            color: macroText.text !== "" && !backend.typeable(macroText.text) ? win.ui.error : win.ui.muted
                            text: qsTr("Typed as on a US keyboard, without accents. With US International, a quote or ` waits for the next letter.")
                        }

                        Section { visible: !!win.macro; text: qsTr("Pause between keys").toUpperCase() }
                        RowLayout {
                            visible: !!win.macro
                            Layout.fillWidth: true
                            spacing: 12
                            AppSlider {
                                Layout.fillWidth: true
                                from: 10; to: 200; stepSize: 10
                                value: win.macro ? win.macro.delay : 10
                                onMoved: backend.setMacroDelay(win.pickedMacro, value)
                            }
                            Label {
                                Layout.preferredWidth: 60
                                text: qsTr("%1 ms").arg(win.macro ? win.macro.delay : 10)
                                font.pixelSize: 13
                                color: win.ui.foreground
                            }
                        }

                        Section { text: qsTr("Keyboard memory").toUpperCase() }
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: 8
                            radius: 4
                            color: win.ui.button
                            Rectangle {
                                width: parent.width * Math.min(backend.macroUsed / backend.macroSpace, 1)
                                height: parent.height
                                radius: 4
                                color: backend.macroUsed > backend.macroSpace ? win.ui.error : win.ui.accent
                            }
                        }
                        Label {
                            font.pixelSize: 12
                            color: backend.macroUsed > backend.macroSpace ? win.ui.error : win.ui.muted
                            text: backend.macroUsed > backend.macroSpace
                                  ? qsTr("Too long: %1 of %2 bytes. Shorten or remove a macro to save.").arg(backend.macroUsed).arg(backend.macroSpace)
                                  : qsTr("%1 of %2 bytes used, for every macro together.").arg(backend.macroUsed).arg(backend.macroSpace)
                        }

                        AppButton {
                            visible: !!win.macro
                            Layout.topMargin: 12
                            flat: true
                            text: qsTr("Delete macro")
                            onClicked: {
                                backend.removeMacro(win.pickedMacro)
                                win.pickedMacro = Math.min(win.pickedMacro, backend.macros.length - 1)
                            }
                        }
                        Label {
                            Layout.topMargin: 12
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            font.pixelSize: 12
                            color: win.ui.muted
                            text: qsTr("Save to keyboard writes the macros and the keys together. Macros live on the keyboard, so they work on any computer.")
                        }
                    }
                }
            }
        }
    }

    // A short outcome at the bottom, gone after a moment.
    Rectangle {
        id: toast
        property string text
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 22
        width: toastLabel.implicitWidth + 32
        height: 38
        radius: Math.min(6, theme.radius * 2)
        color: win.ui.panel
        border.color: win.ui.border
        opacity: toastTimer.running ? 1 : 0
        visible: opacity > 0
        Behavior on opacity { NumberAnimation { duration: 160 } }
        Label { id: toastLabel; anchors.centerIn: parent; text: toast.text; font.pixelSize: 13; color: win.ui.foreground }
        Timer { id: toastTimer; interval: 2600 }
        Connections {
            target: backend
            function onNotice(message) {
                toast.text = message
                toastTimer.restart()
            }
        }
    }
}
