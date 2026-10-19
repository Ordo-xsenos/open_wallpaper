import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.CheckBox {
    id: control

    implicitHeight: 34
    spacing: 9
    font.pixelSize: 13
    opacity: enabled ? 1 : 0.45

    contentItem: Text {
        text: control.text
        font: control.font
        color: "#edf1f5"
        leftPadding: control.indicator.width + control.spacing
        verticalAlignment: Text.AlignVCenter
        wrapMode: Text.Wrap
        textFormat: Text.PlainText
    }

    indicator: Rectangle {
        implicitWidth: 17
        implicitHeight: 17
        y: (control.height - height) / 2
        radius: 2
        color: control.checked ? "#316e98" : "#171e28"
        border.color: control.activeFocus ? "#a9d9ff" : "#8290a0"

        Canvas {
            anchors.fill: parent
            visible: control.checked
            onPaint: {
                const context = getContext("2d");
                context.strokeStyle = "#ffffff";
                context.lineWidth = 2;
                context.beginPath();
                context.moveTo(4, 8);
                context.lineTo(7, 11);
                context.lineTo(13, 5);
                context.stroke();
            }
        }

    }

}
