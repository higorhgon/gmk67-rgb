import QtQuick
import QtQuick.Shapes

// A line icon drawn on a 24 px grid, in any color and size, the way Hype
// draws its toolbar icons.
Item {
    id: icon

    required property string name
    property color color: theme.colors.foreground

    implicitWidth: 24
    implicitHeight: 24

    readonly property var paths: ({
        "check": "M5 12.5L10 17.5L19 7",
        "close": "M6 6L18 18 M18 6L6 18",
        "keyboard": "M3 6H21V18H3Z M7 10H7.5 M11 10H11.5 M15 10H15.5 M7 14H17",
        "help": "M12 3A9 9 0 1 0 12 21A9 9 0 1 0 12 3 M9.5 9.5A2.5 2.5 0 1 1 12 12V13.5 M12 17V17.2",
        "play": "M8 5V19L19 12Z",
        "stop": "M7 7H17V17H7Z",
        "copy": "M9 9H19V19H9Z M5 15V5H15",
        "refresh": "M19.5 12A7.5 7.5 0 1 1 17.3 6.7 M19.5 4V8H15.5",
        "sparkles": "M12 3L13.8 9.2L20 11L13.8 12.8L12 19L10.2 12.8L4 11L10.2 9.2Z M19 16V20 M17 18H21",
        "plug": "M9 3V8 M15 3V8 M6 8H18V11A6 6 0 0 1 12 17A6 6 0 0 1 6 11Z M12 17V21",
        "lock": "M6 11H18V20H6Z M8.5 11V8A3.5 3.5 0 0 1 15.5 8V11"
    })

    Shape {
        width: 24
        height: 24
        anchors.centerIn: parent
        scale: Math.min(icon.width, icon.height) / 24
        antialiasing: true
        preferredRendererType: Shape.CurveRenderer

        ShapePath {
            fillColor: "transparent"
            strokeColor: icon.color
            strokeWidth: 1.8
            capStyle: ShapePath.RoundCap
            joinStyle: ShapePath.RoundJoin
            PathSvg { path: icon.paths[icon.name] || "" }
        }
    }
}
