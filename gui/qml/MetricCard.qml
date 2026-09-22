// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string label: ""
    property string value: "—"
    property string detail: ""
    property color accentColor: "#7aa2f7"
    property color surfaceColor: "#24283b"
    property color borderColor: "#414868"
    property color textColor: "#c0caf5"
    property color mutedColor: "#9aa5ce"
    property real progress: -1

    implicitWidth: 190
    implicitHeight: 112
    radius: 14
    color: surfaceColor
    border.color: borderColor
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 5

        Label {
            Layout.fillWidth: true
            text: root.label
            color: root.mutedColor
            font.pixelSize: 12
            elide: Text.ElideRight
        }
        Label {
            Layout.fillWidth: true
            text: root.value
            color: root.textColor
            font.pixelSize: 24
            font.weight: Font.DemiBold
            elide: Text.ElideRight
        }
        Label {
            Layout.fillWidth: true
            text: root.detail
            color: root.mutedColor
            font.pixelSize: 11
            elide: Text.ElideRight
        }
        Item {
            Layout.fillWidth: true
            implicitHeight: 5
            visible: root.progress >= 0
            Rectangle {
                anchors.fill: parent
                radius: 3
                color: Qt.rgba(root.borderColor.r, root.borderColor.g,
                               root.borderColor.b, 0.55)
            }
            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, root.progress))
                height: parent.height
                radius: 3
                color: root.accentColor
            }
        }
    }
}
