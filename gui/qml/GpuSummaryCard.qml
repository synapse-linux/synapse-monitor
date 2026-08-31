// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property string label: "GPU"
    property string utilizationLabel: ""
    property string utilizationValue: "—"
    property string memoryLabel: ""
    property string memoryValue: "—"
    property string detail: ""
    property color accentColor: "#9ece6a"
    property color surfaceColor: "#24283b"
    property color borderColor: "#414868"
    property color textColor: "#c0caf5"
    property color mutedColor: "#9aa5ce"
    property real progress: -1

    implicitWidth: 260
    implicitHeight: 112
    radius: 14
    color: root.surfaceColor
    border.color: root.borderColor
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 14
        spacing: 3

        Label {
            Layout.fillWidth: true
            text: root.label
            color: root.mutedColor
            font.pixelSize: 12
            elide: Text.ElideRight
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: 12

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Label {
                    Layout.fillWidth: true
                    text: root.utilizationLabel
                    color: root.mutedColor
                    font.pixelSize: 9
                    elide: Text.ElideRight
                }
                Label {
                    objectName: "gpuSummaryUtilizationValue"
                    Layout.fillWidth: true
                    text: root.utilizationValue
                    color: root.textColor
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
            }
            Rectangle {
                Layout.preferredWidth: 1
                Layout.preferredHeight: 32
                color: root.borderColor
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 0
                Label {
                    Layout.fillWidth: true
                    text: root.memoryLabel
                    color: root.mutedColor
                    font.pixelSize: 9
                    elide: Text.ElideRight
                }
                Label {
                    objectName: "gpuSummaryMemoryValue"
                    Layout.fillWidth: true
                    text: root.memoryValue
                    color: root.textColor
                    font.pixelSize: 18
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
            }
        }
        Label {
            Layout.fillWidth: true
            text: root.detail
            color: root.mutedColor
            font.pixelSize: 10
            elide: Text.ElideRight
        }
        Item {
            Layout.fillWidth: true
            implicitHeight: 4
            visible: root.progress >= 0
            Rectangle {
                anchors.fill: parent
                radius: 2
                color: Qt.rgba(root.borderColor.r, root.borderColor.g,
                               root.borderColor.b, 0.55)
            }
            Rectangle {
                width: parent.width * Math.max(0, Math.min(1, root.progress))
                height: parent.height
                radius: 2
                color: root.accentColor
            }
        }
    }
}
