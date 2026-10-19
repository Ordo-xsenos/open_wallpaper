import QtQuick
import QtQuick.Controls.Basic as Basic

Basic.TextField {
    id: control

    implicitHeight: 34
    leftPadding: 10
    rightPadding: 10
    font.pixelSize: 13
    color: "#edf1f5"
    placeholderTextColor: "#a8b4c3"
    selectionColor: "#316e98"
    selectedTextColor: "#ffffff"

    background: Rectangle {
        radius: 3
        color: "#171e28"
        border.color: control.activeFocus ? "#83bde8" : "#47515e"
    }

}
