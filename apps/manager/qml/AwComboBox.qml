import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.ComboBox {
    id: control

    implicitWidth: 170
    implicitHeight: 34
    leftPadding: 10
    rightPadding: 28
    font.pixelSize: 13
    opacity: enabled ? 1 : 0.45
    palette.text: "#edf1f5"
    palette.windowText: "#edf1f5"
    palette.base: "#242d39"
    palette.window: "#242d39"
    palette.highlight: "#316e98"
    palette.highlightedText: "#ffffff"

    contentItem: Text {
        text: control.displayText
        font: control.font
        color: "#edf1f5"
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        textFormat: Text.PlainText
    }

    background: Rectangle {
        radius: 3
        color: control.hovered ? "#3c4857" : "#303945"
        border.color: control.activeFocus ? "#a9d9ff" : "#495361"
    }

    indicator: Canvas {
        x: control.width - width - 10
        y: (control.height - height) / 2
        width: 8
        height: 5
        onPaint: {
            const context = getContext("2d");
            context.fillStyle = "#d8e4ef";
            context.beginPath();
            context.moveTo(0, 0);
            context.lineTo(8, 0);
            context.lineTo(4, 5);
            context.closePath();
            context.fill();
        }
    }

}
