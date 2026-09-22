// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property var adapter
    property color surfaceColor: "#24283b"
    property color borderColor: "#414868"
    property color textColor: "#c0caf5"
    property color mutedColor: "#9aa5ce"
    property color accentColor: "#7aa2f7"
    property var information: adapter.payload.information || ({})

    function present(value) {
        return value === null || value === undefined || value === "" ? "—" : String(value)
    }
    function bytes(value) {
        if (value === null || value === undefined) return "—"
        const gib = Number(value) / 1073741824
        return gib.toFixed(gib >= 100 ? 0 : 1) + " GiB"
    }
    function uptime(value) {
        if (value === null || value === undefined) return "—"
        let seconds = Math.max(0, Math.trunc(Number(value)))
        const days = Math.trunc(seconds / 86400); seconds %= 86400
        const hours = Math.trunc(seconds / 3600); seconds %= 3600
        const minutes = Math.trunc(seconds / 60)
        return days + "d " + hours + "h " + minutes + "m"
    }

    ScrollView {
        id: informationScroll
        anchors.fill: parent
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: Math.max(580, informationScroll.availableWidth)
            spacing: 14

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 3
                Label { text: qsTrId("synapse.monitor.view.information"); color: root.textColor; font.pixelSize: 22; font.weight: Font.DemiBold }
                Label { Layout.fillWidth: true; text: qsTrId("synapse.monitor.subtitle.information"); color: root.mutedColor; wrapMode: Text.WordWrap }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: width >= 900 ? 2 : 1
                columnSpacing: 12
                rowSpacing: 12

                Rectangle {
                    Layout.fillWidth: true; implicitHeight: osColumn.implicitHeight + 32
                    radius: 14; color: root.surfaceColor; border.color: root.borderColor
                    ColumnLayout {
                        id: osColumn; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16; spacing: 7
                        Label { text: qsTrId("synapse.monitor.info.operating-system"); color: root.accentColor; font.weight: Font.DemiBold }
                        Label { Layout.fillWidth: true; text: root.present(root.information.operatingSystem); color: root.textColor; font.pixelSize: 20; font.weight: Font.DemiBold; wrapMode: Text.WordWrap }
                        Label { Layout.fillWidth: true; text: root.present(root.information.kernel) + "  ·  " + root.present(root.information.architecture); color: root.mutedColor; wrapMode: Text.WrapAnywhere }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true; implicitHeight: hardwareColumn.implicitHeight + 32
                    radius: 14; color: root.surfaceColor; border.color: root.borderColor
                    ColumnLayout {
                        id: hardwareColumn; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16; spacing: 7
                        Label { text: qsTrId("synapse.monitor.info.hardware"); color: root.accentColor; font.weight: Font.DemiBold }
                        Label { Layout.fillWidth: true; text: root.present(root.information.systemVendor) + " " + root.present(root.information.systemModel); color: root.textColor; font.pixelSize: 17; font.weight: Font.DemiBold; wrapMode: Text.WordWrap }
                        Label { Layout.fillWidth: true; text: root.present(root.information.processor); color: root.mutedColor; wrapMode: Text.WordWrap }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true; implicitHeight: memoryColumn.implicitHeight + 32
                    radius: 14; color: root.surfaceColor; border.color: root.borderColor
                    ColumnLayout {
                        id: memoryColumn; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16; spacing: 7
                        Label { text: qsTrId("synapse.monitor.info.memory"); color: root.accentColor; font.weight: Font.DemiBold }
                        Label { text: root.bytes(root.information.memoryTotalBytes); color: root.textColor; font.pixelSize: 28; font.weight: Font.DemiBold }
                        Label { text: qsTrId("synapse.monitor.info.physical-memory"); color: root.mutedColor }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true; implicitHeight: uptimeColumn.implicitHeight + 32
                    radius: 14; color: root.surfaceColor; border.color: root.borderColor
                    ColumnLayout {
                        id: uptimeColumn; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16; spacing: 7
                        Label { text: qsTrId("synapse.monitor.info.uptime"); color: root.accentColor; font.weight: Font.DemiBold }
                        Label { text: root.uptime(root.information.uptimeSeconds); color: root.textColor; font.pixelSize: 28; font.weight: Font.DemiBold }
                        Label { text: qsTrId("synapse.monitor.info.since-boot"); color: root.mutedColor }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: firmwareColumn.implicitHeight + 32
                radius: 14; color: root.surfaceColor; border.color: root.borderColor
                ColumnLayout {
                    id: firmwareColumn; anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 16; spacing: 8
                    Label { text: qsTrId("synapse.monitor.info.firmware"); color: root.accentColor; font.weight: Font.DemiBold }
                    GridLayout {
                        Layout.fillWidth: true; columns: width >= 700 ? 3 : 1; columnSpacing: 18; rowSpacing: 8
                        ColumnLayout { Label { text: qsTrId("synapse.monitor.column.vendor"); color: root.mutedColor; font.pixelSize: 10 } Label { text: root.present(root.information.firmwareVendor); color: root.textColor } }
                        ColumnLayout { Label { text: qsTrId("synapse.monitor.column.version"); color: root.mutedColor; font.pixelSize: 10 } Label { text: root.present(root.information.firmwareVersion); color: root.textColor } }
                        ColumnLayout { Label { text: qsTrId("synapse.monitor.column.date"); color: root.mutedColor; font.pixelSize: 10 } Label { text: root.present(root.information.firmwareDate); color: root.textColor } }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: privacyText.implicitHeight + 28
                radius: 12
                color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.10)
                border.color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.35)
                Label {
                    id: privacyText
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top; anchors.margins: 14
                    text: qsTrId("synapse.monitor.information.privacy")
                    color: root.mutedColor
                    font.pixelSize: 11
                    wrapMode: Text.WordWrap
                }
            }
            Item { Layout.fillWidth: true; implicitHeight: 4 }
        }
    }
}
