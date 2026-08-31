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
    property color borderColor: "#414868"
    property color textColor: "#c0caf5"
    property color mutedColor: "#9aa5ce"
    property color accentColor: "#7aa2f7"
    property color greenColor: "#9ece6a"
    property color yellowColor: "#e0af68"
    property color redColor: "#f7768e"
    property color purpleColor: "#bb9af7"
    property var frameData: adapter.payload || ({})
    property var cpu: frameData.cpu || ({})
    property var memory: frameData.memory || ({})
    property var gpu: frameData.gpu || ({})
    property var gpus: frameData.gpus || ({})
    property var primaryGpu: gpus.rows && gpus.rows.length ? gpus.rows[0] : ({})
    property var thermals: frameData.thermals || ({})
    property var disks: frameData.disks || ({})
    property var network: frameData.network || ({})
    property var history: frameData.history || ({})

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
    function decimalBytes(value) {
        if (value === null || value === undefined) return "—"
        const units = ["B", "kB", "MB", "GB", "TB"]
        let amount = Math.max(0, Number(value)); let unit = 0
        while (amount >= 1000 && unit < units.length - 1) { amount /= 1000; ++unit }
        return (unit ? amount.toFixed(1) : amount.toFixed(0)) + " " + units[unit]
    }
    function observedMemoryValue() {
        return root.memory.observedFootprintAvailable
             ? root.decimalBytes(root.memory.observedFootprintBytes)
             : root.bytes(root.memory.usedBytes)
    }
    function observedMemoryDetail() {
        const available = qsTrId("synapse.monitor.metric.available") + " "
                        + root.decimalBytes(root.memory.availableBytes)
        return root.memory.observedFootprintAvailable
             ? available + " · " + qsTrId("synapse.monitor.memory.pss-plus-gpu")
             : available
    }
    function gpuMemoryValue(gpuRow) {
        if (gpuRow.memoryUsedBytes !== null
            && gpuRow.memoryUsedBytes !== undefined)
            return root.bytes(gpuRow.memoryUsedBytes)
        if (gpuRow.memoryKind === "shared")
            return qsTrId("synapse.monitor.value.shared")
        return qsTrId("synapse.monitor.value.unavailable")
    }
    function gpuMemoryDetail(gpuRow) {
        if (gpuRow.memoryKind === "shared"
            && gpuRow.memoryUsedBytes !== null
            && gpuRow.memoryUsedBytes !== undefined)
            return qsTrId("synapse.monitor.gpu-memory.shared-measured")
        if (gpuRow.memoryUsedBytes !== null
            && gpuRow.memoryUsedBytes !== undefined
            && gpuRow.memoryTotalBytes !== null
            && gpuRow.memoryTotalBytes !== undefined)
            return root.bytes(gpuRow.memoryTotalBytes) + " · "
                   + qsTrId("synapse.monitor.gpu-memory.driver-reported")
        if (gpuRow.memoryKind === "shared")
            return qsTrId("synapse.monitor.gpu-memory.shared-unavailable")
        if (root.gpu.present)
            return qsTrId("synapse.monitor.gpu-memory.unavailable")
        return qsTrId("synapse.monitor.gpu-memory.no-device")
    }
    function gpuMemoryLine(gpuRow) {
        if (gpuRow.memoryKind === "shared"
            && gpuRow.memoryUsedBytes !== null
            && gpuRow.memoryUsedBytes !== undefined)
            return root.bytes(gpuRow.memoryUsedBytes) + " · "
                   + qsTrId("synapse.monitor.value.shared")
        if (gpuRow.memoryUsedBytes !== null
            && gpuRow.memoryUsedBytes !== undefined
            && gpuRow.memoryTotalBytes !== null
            && gpuRow.memoryTotalBytes !== undefined)
            return root.bytes(gpuRow.memoryUsedBytes) + " / "
                   + root.bytes(gpuRow.memoryTotalBytes)
        if (gpuRow.memoryKind === "shared")
            return qsTrId("synapse.monitor.value.shared") + " · "
                   + qsTrId("synapse.monitor.value.unavailable")
        return qsTrId("synapse.monitor.value.unavailable")
    }
    function temperature(value) {
        return value === null || value === undefined ? "—"
             : (Number(value) / 1000).toFixed(1) + " °C"
    }
    function frequency(value) {
        return value === null || value === undefined ? "—"
             : (Number(value) / 1000000000).toFixed(2) + " GHz"
    }
    function power(value) {
        return value === null || value === undefined ? "—"
             : (Number(value) / 1000000).toFixed(1) + " W"
    }
    function maximum(values, floorValue) {
        let result = floorValue
        if (!values) return result
        for (let index = 0; index < values.length; ++index)
            if (values[index] !== null && values[index] !== undefined)
                result = Math.max(result, Number(values[index]))
        return result
    }

    ScrollView {
        id: performanceScroll
        anchors.fill: parent
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            width: Math.max(620, performanceScroll.availableWidth)
            spacing: 14

            GridLayout {
                Layout.fillWidth: true
                columns: width >= 900 ? 4 : 2
                columnSpacing: 10
                rowSpacing: 10

                MetricCard {
                    Layout.fillWidth: true
                    label: qsTrId("synapse.monitor.metric.cpu")
                    value: root.percent(root.cpu.busyPercentMilli)
                    detail: (root.cpu.logicalProcessorCount || "—") + " "
                            + qsTrId("synapse.monitor.metric.logical")
                    progress: root.cpu.busyPercentMilli === undefined ? -1
                              : Number(root.cpu.busyPercentMilli) / 100000
                    accentColor: root.accentColor; surfaceColor: root.surfaceColor
                    borderColor: root.borderColor; textColor: root.textColor; mutedColor: root.mutedColor
                }
                MetricCard {
                    Layout.fillWidth: true
                    label: root.memory.observedFootprintAvailable
                           ? qsTrId("synapse.monitor.metric.memory-observed")
                           : qsTrId("synapse.monitor.metric.memory")
                    value: root.observedMemoryValue()
                    detail: root.observedMemoryDetail()
                    progress: root.memory.totalBytes
                              ? Number(root.memory.observedFootprintAvailable
                                       ? root.memory.observedFootprintBytes
                                       : root.memory.usedBytes)
                                / Number(root.memory.totalBytes) : -1
                    accentColor: root.purpleColor; surfaceColor: root.surfaceColor
                    borderColor: root.borderColor; textColor: root.textColor; mutedColor: root.mutedColor
                }
                MetricCard {
                    Layout.fillWidth: true
                    label: qsTrId("synapse.monitor.metric.gpu")
                    value: root.percent(root.gpu.busyPercentMilli)
                    detail: root.gpu.present ? qsTrId("synapse.monitor.status.detected")
                                             : qsTrId("synapse.monitor.value.unavailable")
                    progress: root.gpu.busyPercentMilli === null
                              || root.gpu.busyPercentMilli === undefined ? -1
                              : Number(root.gpu.busyPercentMilli) / 100000
                    accentColor: root.greenColor; surfaceColor: root.surfaceColor
                    borderColor: root.borderColor; textColor: root.textColor; mutedColor: root.mutedColor
                }
                MetricCard {
                    Layout.fillWidth: true
                    label: qsTrId("synapse.monitor.metric.gpu-memory")
                    value: root.gpuMemoryValue(root.primaryGpu)
                    detail: root.gpuMemoryDetail(root.primaryGpu)
                    progress: root.primaryGpu.memoryTotalBytes
                              && root.primaryGpu.memoryUsedBytes !== null
                              && root.primaryGpu.memoryUsedBytes !== undefined
                              ? Number(root.primaryGpu.memoryUsedBytes)
                                / Number(root.primaryGpu.memoryTotalBytes) : -1
                    accentColor: root.greenColor; surfaceColor: root.surfaceColor
                    borderColor: root.borderColor; textColor: root.textColor; mutedColor: root.mutedColor
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: width >= 900 ? 2 : 1
                columnSpacing: 10
                rowSpacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 175
                    radius: 14; color: root.surfaceColor; border.color: root.borderColor
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 14; spacing: 7
                        RowLayout {
                            Layout.fillWidth: true
                            Label { Layout.fillWidth: true; text: qsTrId("synapse.monitor.chart.cpu-memory"); color: root.textColor; font.weight: Font.DemiBold }
                            Label { text: qsTrId("synapse.monitor.chart.last60"); color: root.mutedColor; font.pixelSize: 10 }
                        }
                        Sparkline {
                            Layout.fillWidth: true; Layout.fillHeight: true
                            values: root.history.cpuPercentMilli || []
                            maximum: 100000; lineColor: root.accentColor; gridColor: root.borderColor
                        }
                        Sparkline {
                            Layout.fillWidth: true; Layout.preferredHeight: 42
                            values: root.history.memoryPercentMilli || []
                            maximum: 100000; lineColor: root.purpleColor; gridColor: root.borderColor
                        }
                    }
                }
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 175
                    radius: 14; color: root.surfaceColor; border.color: root.borderColor
                    ColumnLayout {
                        anchors.fill: parent; anchors.margins: 14; spacing: 7
                        RowLayout {
                            Layout.fillWidth: true
                            Label { Layout.fillWidth: true; text: qsTrId("synapse.monitor.chart.io"); color: root.textColor; font.weight: Font.DemiBold }
                            Label { text: qsTrId("synapse.monitor.chart.null-gap"); color: root.mutedColor; font.pixelSize: 10 }
                        }
                        Sparkline {
                            Layout.fillWidth: true; Layout.fillHeight: true
                            values: root.history.diskBytesPerSecond || []
                            maximum: root.maximum(values, 1); lineColor: root.yellowColor; gridColor: root.borderColor
                        }
                        Sparkline {
                            Layout.fillWidth: true; Layout.preferredHeight: 42
                            values: root.history.networkBytesPerSecond || []
                            maximum: root.maximum(values, 1); lineColor: "#7dcfff"; gridColor: root.borderColor
                        }
                    }
                }
            }

            Rectangle {
                Layout.fillWidth: true
                implicitHeight: cpuColumn.implicitHeight + 28
                radius: 14; color: root.surfaceColor; border.color: root.borderColor
                ColumnLayout {
                    id: cpuColumn
                    anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                    anchors.margins: 14; spacing: 8
                    Label { text: qsTrId("synapse.monitor.section.logical-cpu"); color: root.textColor; font.weight: Font.DemiBold }
                    Flow {
                        Layout.fillWidth: true
                        spacing: 6
                        Repeater {
                            model: root.cpu.logicalProcessors || []
                            delegate: Rectangle {
                                id: cpuCore
                                required property var modelData
                                required property int index
                                width: 92; height: 34; radius: 8
                                color: root.alternateColor; border.color: root.borderColor
                                Row {
                                    anchors.centerIn: parent; spacing: 6
                                    Label { text: cpuCore.index; color: root.mutedColor; font.pixelSize: 10 }
                                    Label { text: root.percent(cpuCore.modelData); color: root.textColor; font.pixelSize: 11; font.weight: Font.DemiBold }
                                }
                            }
                        }
                    }
                }
            }

            Label {
                text: qsTrId("synapse.monitor.section.gpus")
                color: root.textColor
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }
            Label {
                Layout.fillWidth: true
                visible: !(root.gpus.rows && root.gpus.rows.length)
                text: qsTrId("synapse.monitor.value.unavailable")
                color: root.mutedColor
            }
            Repeater {
                model: root.gpus.rows || []
                delegate: Rectangle {
                    id: gpuRow
                    required property var modelData
                    Layout.fillWidth: true
                    implicitHeight: gpuColumn.implicitHeight + 28
                    radius: 14; color: root.surfaceColor; border.color: root.borderColor
                    ColumnLayout {
                        id: gpuColumn
                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                        anchors.margins: 14; spacing: 8
                        RowLayout {
                            Layout.fillWidth: true
                            Label { Layout.fillWidth: true; text: gpuRow.modelData.model || ("GPU " + gpuRow.modelData.card); color: root.textColor; font.weight: Font.DemiBold; elide: Text.ElideRight }
                            Label { text: gpuRow.modelData.driver || "—"; color: root.mutedColor; font.pixelSize: 11 }
                        }
                        GridLayout {
                            Layout.fillWidth: true; columns: width >= 850 ? 4 : 2; columnSpacing: 14; rowSpacing: 5
                            Label { text: qsTrId("synapse.monitor.metric.utilization") + "  " + root.percent(gpuRow.modelData.utilizationPercentMilli); color: root.mutedColor }
                            Label {
                                text: (gpuRow.modelData.memoryKind === "shared"
                                       ? qsTrId("synapse.monitor.metric.gpu-memory")
                                       : qsTrId("synapse.monitor.metric.vram"))
                                      + "  " + root.gpuMemoryLine(gpuRow.modelData)
                                color: root.mutedColor
                            }
                            Label { text: qsTrId("synapse.monitor.metric.temperature") + "  " + root.temperature(gpuRow.modelData.temperatureMillidegreesCelsius); color: root.mutedColor }
                            Label { text: qsTrId("synapse.monitor.metric.power") + "  " + root.power(gpuRow.modelData.powerMicrowatts); color: root.mutedColor }
                            Label { text: qsTrId("synapse.monitor.metric.core-clock") + "  " + root.frequency(gpuRow.modelData.coreClockHz); color: root.mutedColor }
                            Label { text: qsTrId("synapse.monitor.metric.memory-clock") + "  " + root.frequency(gpuRow.modelData.memoryClockHz); color: root.mutedColor }
                            Label { text: qsTrId("synapse.monitor.metric.fan") + "  " + (gpuRow.modelData.fanRpm === null || gpuRow.modelData.fanRpm === undefined ? "—" : gpuRow.modelData.fanRpm + " RPM"); color: root.mutedColor }
                            Label {
                                text: gpuRow.modelData.memoryKind === "shared"
                                      ? qsTrId("synapse.monitor.gpu-memory.shared-kind")
                                      : (gpuRow.modelData.memoryKind === "driver-reported-vram"
                                         ? qsTrId("synapse.monitor.gpu-memory.driver-kind")
                                         : qsTrId("synapse.monitor.value.unavailable"))
                                color: root.mutedColor
                                font.pixelSize: 10
                            }
                        }
                    }
                }
            }

            GridLayout {
                Layout.fillWidth: true
                columns: width >= 900 ? 2 : 1
                columnSpacing: 10; rowSpacing: 10

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: thermalColumn.implicitHeight + 28
                    radius: 14; color: root.surfaceColor; border.color: root.borderColor
                    ColumnLayout {
                        id: thermalColumn
                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                        anchors.margins: 14; spacing: 7
                        Label { text: qsTrId("synapse.monitor.section.thermals"); color: root.textColor; font.weight: Font.DemiBold }
                        Repeater {
                            model: root.thermals.temperatures || []
                            delegate: RowLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                Label { Layout.fillWidth: true; text: (modelData.label || "—") + "  ·  " + (modelData.class || ""); color: root.mutedColor; elide: Text.ElideRight }
                                Label { text: root.temperature(modelData.temperatureMillidegreesCelsius); color: root.textColor; font.weight: Font.DemiBold }
                            }
                        }
                        Repeater {
                            model: root.thermals.fans || []
                            delegate: RowLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                Label { Layout.fillWidth: true; text: modelData.label || modelData.source || "—"; color: root.mutedColor; elide: Text.ElideRight }
                                Label { text: modelData.rpm === null || modelData.rpm === undefined ? "—" : modelData.rpm + " RPM"; color: root.textColor }
                            }
                        }
                    }
                }

                Rectangle {
                    Layout.fillWidth: true
                    implicitHeight: ioColumn.implicitHeight + 28
                    radius: 14; color: root.surfaceColor; border.color: root.borderColor
                    ColumnLayout {
                        id: ioColumn
                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                        anchors.margins: 14; spacing: 7
                        Label { text: qsTrId("synapse.monitor.section.storage-network"); color: root.textColor; font.weight: Font.DemiBold }
                        Repeater {
                            model: root.disks.rows || []
                            delegate: RowLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                Label { Layout.fillWidth: true; text: modelData.name || "—"; color: root.mutedColor }
                                Label { text: "↓ " + root.bytes(modelData.readBytesPerSecond) + "/s  ↑ " + root.bytes(modelData.writeBytesPerSecond) + "/s"; color: root.textColor; font.pixelSize: 11 }
                            }
                        }
                        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: root.borderColor }
                        Repeater {
                            model: root.network.rows || []
                            delegate: RowLayout {
                                required property var modelData
                                Layout.fillWidth: true
                                Label { Layout.fillWidth: true; text: modelData.name || "—"; color: root.mutedColor }
                                Label { text: "↓ " + root.bytes(modelData.receiveBytesPerSecond) + "/s  ↑ " + root.bytes(modelData.transmitBytesPerSecond) + "/s"; color: root.textColor; font.pixelSize: 11 }
                            }
                        }
                    }
                }
            }

            Label {
                Layout.fillWidth: true
                text: qsTrId("synapse.monitor.performance.no-inference")
                color: root.mutedColor
                font.pixelSize: 10
                wrapMode: Text.WordWrap
            }
            Item { Layout.fillWidth: true; implicitHeight: 4 }
        }
    }
}
