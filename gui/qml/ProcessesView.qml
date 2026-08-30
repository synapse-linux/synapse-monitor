// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property var adapter
    property color backgroundColor: "#1a1b26"
    property color surfaceColor: "#24283b"
    property color alternateColor: "#202435"
    property color hoverColor: "#292e42"
    property color borderColor: "#414868"
    property color textColor: "#c0caf5"
    property color mutedColor: "#9aa5ce"
    property color accentColor: "#7aa2f7"
    property color greenColor: "#9ece6a"
    property color yellowColor: "#e0af68"
    property color purpleColor: "#bb9af7"
    property var summary: adapter.payload.summary || ({})
    property var cpu: summary.cpu || ({})
    property var memory: summary.memory || ({})
    property var gpu: summary.gpu || ({})
    property var disk: summary.disk || ({})
    property var network: summary.network || ({})

    function percent(value) {
        return value === null || value === undefined ? "—"
             : (Number(value) / 1000).toFixed(1) + "%"
    }
    function bytes(value) {
        if (value === null || value === undefined) return "—"
        const units = ["B", "KiB", "MiB", "GiB", "TiB"]
        let amount = Math.max(0, Number(value)); let unit = 0
        while (amount >= 1024 && unit < units.length - 1) { amount /= 1024; ++unit }
        return (unit ? amount.toFixed(1) : amount.toFixed(0)) + " " + units[unit]
    }
    function identifierLabel(identifier) {
        return qsTrId("synapse.monitor.id." + identifier)
    }
    function columns() {
        return [
            { key: "name", label: qsTrId("synapse.monitor.column.name"), width: 220 },
            { key: "class", label: qsTrId("synapse.monitor.column.class"), width: 110 },
            { key: "pid", label: "PID", width: 84, align: "right", format: "pid" },
            { key: "uid", label: "UID", width: 76, align: "right", format: "integer" },
            { key: "state", label: qsTrId("synapse.monitor.column.state"), width: 82 },
            { key: "threads", label: qsTrId("synapse.monitor.column.threads"), width: 84, align: "right", format: "integer" },
            { key: "cpuPercentMilli", label: "CPU", width: 92, align: "right", format: "percent" },
            { key: "residentBytes", label: qsTrId("synapse.monitor.column.memory"), width: 110, align: "right", format: "bytes" },
            { key: "readBytesPerSecond", label: qsTrId("synapse.monitor.column.read"), width: 110, align: "right", format: "rate" },
            { key: "writeBytesPerSecond", label: qsTrId("synapse.monitor.column.write"), width: 110, align: "right", format: "rate" }
        ]
    }

    Timer {
        id: filterTimer
        interval: 280
        repeat: false
        onTriggered: root.adapter.setFilter(filterField.text)
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 12

        GridLayout {
            Layout.fillWidth: true
            columns: width >= 1040 ? 5 : (width >= 650 ? 3 : 2)
            columnSpacing: 10
            rowSpacing: 10

            MetricCard {
                Layout.fillWidth: true
                label: qsTrId("synapse.monitor.metric.cpu")
                value: root.percent(root.cpu.busyPercentMilli)
                detail: (root.cpu.logicalProcessors || "—") + " " + qsTrId("synapse.monitor.metric.logical")
                progress: root.cpu.busyPercentMilli === undefined ? -1 : Number(root.cpu.busyPercentMilli) / 100000
                accentColor: root.accentColor; surfaceColor: root.surfaceColor; borderColor: root.borderColor
                textColor: root.textColor; mutedColor: root.mutedColor
            }
            MetricCard {
                Layout.fillWidth: true
                label: qsTrId("synapse.monitor.metric.memory")
                value: root.bytes(root.memory.usedBytes)
                detail: qsTrId("synapse.monitor.metric.of") + " " + root.bytes(root.memory.totalBytes)
                progress: root.memory.totalBytes ? Number(root.memory.usedBytes) / Number(root.memory.totalBytes) : -1
                accentColor: root.purpleColor; surfaceColor: root.surfaceColor; borderColor: root.borderColor
                textColor: root.textColor; mutedColor: root.mutedColor
            }
            MetricCard {
                Layout.fillWidth: true
                label: qsTrId("synapse.monitor.metric.gpu")
                value: root.percent(root.gpu.busyPercentMilli)
                detail: root.gpu.present ? root.bytes(root.gpu.memoryUsedBytes) : qsTrId("synapse.monitor.value.unavailable")
                progress: root.gpu.busyPercentMilli === null || root.gpu.busyPercentMilli === undefined
                          ? -1 : Number(root.gpu.busyPercentMilli) / 100000
                accentColor: root.greenColor; surfaceColor: root.surfaceColor; borderColor: root.borderColor
                textColor: root.textColor; mutedColor: root.mutedColor
            }
            MetricCard {
                Layout.fillWidth: true
                label: qsTrId("synapse.monitor.metric.disk")
                value: root.bytes((Number(root.disk.readBytesPerSecond || 0)
                                   + Number(root.disk.writeBytesPerSecond || 0))) + "/s"
                detail: (root.disk.devices || 0) + " " + qsTrId("synapse.monitor.metric.devices")
                accentColor: root.yellowColor; surfaceColor: root.surfaceColor; borderColor: root.borderColor
                textColor: root.textColor; mutedColor: root.mutedColor
            }
            MetricCard {
                Layout.fillWidth: true
                label: qsTrId("synapse.monitor.metric.network")
                value: root.bytes((Number(root.network.receiveBytesPerSecond || 0)
                                   + Number(root.network.transmitBytesPerSecond || 0))) + "/s"
                detail: "↓ " + root.bytes(root.network.receiveBytesPerSecond)
                        + "  ↑ " + root.bytes(root.network.transmitBytesPerSecond)
                accentColor: "#7dcfff"; surfaceColor: root.surfaceColor; borderColor: root.borderColor
                textColor: root.textColor; mutedColor: root.mutedColor
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8

            TextField {
                id: filterField
                Layout.fillWidth: true
                Layout.maximumWidth: 380
                visible: root.adapter.filterSupported
                placeholderText: qsTrId("synapse.monitor.action.filter")
                color: root.textColor
                placeholderTextColor: root.mutedColor
                selectByMouse: true
                maximumLength: 64
                validator: RegularExpressionValidator { regularExpression: /^[\x20-\x7e]{0,64}$/ }
                onTextEdited: filterTimer.restart()
                background: Rectangle {
                    radius: 9; color: root.surfaceColor; border.color: root.borderColor
                }
            }
            Label {
                text: qsTrId("synapse.monitor.action.sort")
                color: root.mutedColor
                font.pixelSize: 11
            }
            SynapseComboBox {
                id: sortBox
                model: root.adapter.sortIds
                currentIndex: Math.max(0, root.adapter.sortIds.indexOf(root.adapter.sortId))
                textForValue: function(value) { return root.identifierLabel(value) }
                onActivated: root.adapter.setSortId(String(model[currentIndex]))
                implicitWidth: 150
                surfaceColor: root.surfaceColor; hoverColor: root.hoverColor
                borderColor: root.borderColor; textColor: root.textColor
                mutedColor: root.mutedColor; accentColor: root.accentColor
            }
            Label {
                text: qsTrId("synapse.monitor.action.group")
                color: root.mutedColor
                font.pixelSize: 11
                visible: groupBox.visible
            }
            SynapseComboBox {
                id: groupBox
                model: root.adapter.groupIds
                visible: count > 0
                currentIndex: Math.max(0, root.adapter.groupIds.indexOf(root.adapter.groupId))
                textForValue: function(value) { return root.identifierLabel(value) }
                onActivated: root.adapter.setGroupId(String(model[currentIndex]))
                implicitWidth: 140
                surfaceColor: root.surfaceColor; hoverColor: root.hoverColor
                borderColor: root.borderColor; textColor: root.textColor
                mutedColor: root.mutedColor; accentColor: root.accentColor
            }
            Item { Layout.fillWidth: true }
            Label {
                text: root.adapter.sequence >= 0 ? "#" + root.adapter.sequence : "—"
                color: root.mutedColor
                font.pixelSize: 11
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            DataTable {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: 360
                tableModel: root.adapter.rows
                columnDefinitions: root.columns()
                surfaceColor: root.surfaceColor; alternateColor: root.alternateColor
                hoverColor: root.hoverColor; borderColor: root.borderColor
                textColor: root.textColor; mutedColor: root.mutedColor; accentColor: root.accentColor
                onRowActivated: function(record) {
                    root.adapter.inspectProcess(Number(record.pid), Number(record.startTicks))
                }
            }

            ProcessInspector {
                Layout.preferredWidth: root.width >= 900 ? 360 : 280
                Layout.minimumWidth: 240
                Layout.fillHeight: true
                visible: root.adapter.inspectionBusy
                         || Object.keys(root.adapter.inspection).length > 0
                         || root.adapter.inspectionErrorId.length > 0
                payload: root.adapter.inspection
                busy: root.adapter.inspectionBusy
                errorId: root.adapter.inspectionErrorId
                surfaceColor: root.surfaceColor; backgroundColor: root.backgroundColor
                borderColor: root.borderColor; textColor: root.textColor
                mutedColor: root.mutedColor; accentColor: root.accentColor
                onCloseRequested: root.adapter.closeInspection()
            }
        }
    }
}
