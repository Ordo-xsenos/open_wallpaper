import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.Button {
    id: control

    implicitWidth: Math.max(80, contentItem.implicitWidth + leftPadding + rightPadding)
    implicitHeight: 34
    leftPadding: 14
    rightPadding: 14
    topPadding: 7
    bottomPadding: 7
    hoverEnabled: true
    font.pixelSize: 13
    opacity: enabled ? 1 : 0.45

    contentItem: Text {
        text: control.text
        font: control.font
        color: "#edf1f5"
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
        textFormat: Text.PlainText
    }

    background: Rectangle {
        radius: 3
        color: control.down ? "#255d85" : control.highlighted || control.checked ? "#316e98" : control.hovered ? "#414b58" : "#303945"
        border.width: 1
        border.color: control.activeFocus ? "#a9d9ff" : control.highlighted || control.checked ? "#528eb9" : "#495361"
    }

}
