import QtQuick
import QtQuick.Controls

// A drop-down in the look of AppTextField: the theme's background inside a
// border that takes the accent while open. The model is a list of objects
// shown by textRole.
ComboBox {
    id: box

    readonly property var ui: theme.colors

    implicitHeight: 36
    font.pixelSize: 13

    contentItem: Label {
        leftPadding: 12
        rightPadding: 28
        text: box.displayText
        font: box.font
        color: box.ui.foreground
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
    indicator: Label {
        x: box.width - width - 12
        anchors.verticalCenter: parent.verticalCenter
        text: "▾"
        color: box.ui.muted
    }
    background: Rectangle {
        color: box.ui.background
        radius: Math.min(4, theme.radius)
        border.color: box.activeFocus || box.popup.visible ? box.ui.accent : box.ui.border
    }
    delegate: ItemDelegate {
        id: option
        required property var modelData
        required property int index
        width: ListView.view ? ListView.view.width : box.width
        height: 30
        highlighted: box.highlightedIndex === index
        contentItem: Label {
            leftPadding: 4
            text: option.modelData[box.textRole]
            font: box.font
            color: option.highlighted ? box.ui.selectionText : box.ui.foreground
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: Math.min(3, theme.radius)
            color: option.highlighted ? box.ui.selection : option.hovered ? box.ui.hover : "transparent"
        }
    }
    popup: Popup {
        y: box.height + 2
        width: box.width
        height: Math.min(contentItem.implicitHeight + 6, 320)
        padding: 3
        contentItem: ListView {
            id: options
            clip: true
            implicitHeight: contentHeight
            model: box.popup.visible ? box.delegateModel : null
            currentIndex: box.highlightedIndex
            boundsBehavior: Flickable.StopAtBounds
            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }
            SmoothWheel { view: options }
        }
        background: Rectangle {
            color: box.ui.panel
            radius: Math.min(4, theme.radius)
            border.color: box.ui.border
        }
    }
}
