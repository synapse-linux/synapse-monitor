// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    required property var adapter
    required property string viewId
    property color surfaceColor: "#24283b"
    property color alternateColor: "#202435"
    property color hoverColor: "#292e42"
    property color borderColor: "#414868"
    property color textColor: "#c0caf5"
    property color mutedColor: "#9aa5ce"
    property color accentColor: "#7aa2f7"
    property var coverage: adapter.payload.coverage || ({})

    function columns() {
        if (viewId === "services") return [
            { key: "name", sortId: "name", label: qsTrId("synapse.monitor.column.name"), width: 230 },
            { key: "description", sortId: "description", label: qsTrId("synapse.monitor.column.description"), width: 300 },
            { key: "status", sortId: "status", label: qsTrId("synapse.monitor.column.status"), width: 110 },
            { key: "startup", sortId: "startup", label: qsTrId("synapse.monitor.column.startup"), width: 110 },
            { key: "pid", sortId: "pid", label: "PID", width: 82, align: "right", format: "pid" },
            { key: "user", sortId: "user", label: qsTrId("synapse.monitor.column.user"), width: 110 },
            { key: "executable", sortId: "executable", label: qsTrId("synapse.monitor.column.executable"), width: 220 }
        ]
        if (viewId === "startup") return [
            { key: "name", sortId: "name", label: qsTrId("synapse.monitor.column.name"), width: 230 },
            { key: "publisher", sortId: "publisher", label: qsTrId("synapse.monitor.column.publisher"), width: 180 },
            { key: "status", sortId: "status", label: qsTrId("synapse.monitor.column.status"), width: 110 },
            { key: "type", sortId: "type", label: qsTrId("synapse.monitor.column.type"), width: 100 },
            { key: "location", sortId: "location", label: qsTrId("synapse.monitor.column.location"), width: 180 },
            { key: "command", sortId: "command", label: qsTrId("synapse.monitor.column.command"), width: 280 }
        ]
        return [
            { key: "protocol", sortId: "protocol", label: qsTrId("synapse.monitor.column.protocol"), width: 110 },
            { key: "local", sortId: "local", label: qsTrId("synapse.monitor.column.local"), width: 230 },
            { key: "remote", sortId: "remote", label: qsTrId("synapse.monitor.column.remote"), width: 230 },
            { key: "state", sortId: "status", label: qsTrId("synapse.monitor.column.state"), width: 130 },
            { key: "pid", sortId: "pid", label: "PID", width: 82, align: "right", format: "pid" },
            { key: "process", sortId: "process", label: qsTrId("synapse.monitor.column.process"), width: 180 }
        ]
    }

    function identifierLabel(identifier) {
        return qsTrId("synapse.monitor.id." + identifier)
    }
    function title() { return qsTrId("synapse.monitor.view." + viewId) }
    function subtitle() {
        if (viewId === "services")
            return qsTrId("synapse.monitor.subtitle.services")
        if (viewId === "startup")
            return qsTrId("synapse.monitor.subtitle.startup")
        return qsTrId("synapse.monitor.subtitle.connections")
    }
    function coverageText() {
        if (coverage.rowsReturned !== undefined)
            return coverage.rowsReturned + " / " + (coverage.rowsMatched || coverage.rowsSeen || 0)
        return "—"
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

        RowLayout {
            Layout.fillWidth: true
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2
                Label { text: root.title(); color: root.textColor; font.pixelSize: 20; font.weight: Font.DemiBold }
                Label { Layout.fillWidth: true; text: root.subtitle(); color: root.mutedColor; font.pixelSize: 11; wrapMode: Text.WordWrap }
            }
            Rectangle {
                implicitWidth: coverageLabel.implicitWidth + 22
                implicitHeight: 30
                radius: 15
                color: root.surfaceColor
                border.color: root.borderColor
                Label { id: coverageLabel; anchors.centerIn: parent; text: root.coverageText(); color: root.accentColor; font.pixelSize: 11; font.weight: Font.DemiBold }
            }
        }

        RowLayout {
            Layout.fillWidth: true
            spacing: 8
            TextField {
                id: filterField
                Layout.fillWidth: true
                Layout.maximumWidth: 420
                placeholderText: qsTrId("synapse.monitor.action.filter")
                color: root.textColor
                placeholderTextColor: root.mutedColor
                selectByMouse: true
                maximumLength: 64
                validator: RegularExpressionValidator { regularExpression: /^[\x20-\x7e]{0,64}$/ }
                onTextEdited: filterTimer.restart()
                background: Rectangle { radius: 9; color: root.surfaceColor; border.color: root.borderColor }
            }
            Label { text: qsTrId("synapse.monitor.action.sort"); color: root.mutedColor; font.pixelSize: 11 }
            SynapseComboBox {
                id: sortBox
                model: root.adapter.sortIds
                currentIndex: Math.max(0, root.adapter.sortIds.indexOf(root.adapter.sortId))
                textForValue: function(value) { return root.identifierLabel(value) }
                onActivated: root.adapter.setSortId(String(model[currentIndex]))
                implicitWidth: 170
                surfaceColor: root.surfaceColor; hoverColor: root.hoverColor
                borderColor: root.borderColor; textColor: root.textColor
                mutedColor: root.mutedColor; accentColor: root.accentColor
            }
            Item { Layout.fillWidth: true }
            Label { text: root.adapter.sequence >= 0 ? "#" + root.adapter.sequence : "—"; color: root.mutedColor; font.pixelSize: 11 }
        }

        DataTable {
            Layout.fillWidth: true
            Layout.fillHeight: true
            tableModel: root.adapter.rows
            columnDefinitions: root.columns()
            sortableIds: root.adapter.sortIds
            activeSortId: root.adapter.sortId
            surfaceColor: root.surfaceColor; alternateColor: root.alternateColor
            hoverColor: root.hoverColor; borderColor: root.borderColor
            textColor: root.textColor; mutedColor: root.mutedColor; accentColor: root.accentColor
            onSortRequested: function(sortId) { root.adapter.setSortId(sortId) }
        }

        Label {
            Layout.fillWidth: true
            text: qsTrId("synapse.monitor.readonly." + root.viewId)
            color: root.mutedColor
            font.pixelSize: 10
            wrapMode: Text.WordWrap
        }
    }
}
