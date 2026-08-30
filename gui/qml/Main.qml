// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window

ApplicationWindow {
    id: window
    objectName: "synapseMonitorWindow"
    width: 1280
    height: 800
    minimumWidth: 700
    minimumHeight: 480
    visible: true
    title: qsTrId("synapse.monitor.title")
    color: backgroundColor

    readonly property color backgroundColor: "#1a1b26"
    readonly property color sidebarColor: "#13141c"
    readonly property color surfaceColor: "#24283b"
    readonly property color alternateColor: "#202435"
    readonly property color hoverColor: "#292e42"
    readonly property color borderColor: "#414868"
    readonly property color accentColor: "#7aa2f7"
    readonly property color textPrimary: "#c0caf5"
    readonly property color textMuted: "#9aa5ce"
    readonly property color greenColor: "#9ece6a"
    readonly property color yellowColor: "#e0af68"
    readonly property color redColor: "#f7768e"
    readonly property color purpleColor: "#bb9af7"
    readonly property bool compact: width < 920

    function viewLabel(id) { return qsTrId("synapse.monitor.view." + id) }
    function viewMark(id) {
        if (id === "processes") return "P"
        if (id === "performance") return "◌"
        if (id === "services") return "S"
        if (id === "startup") return "A"
        if (id === "connections") return "C"
        return "i"
    }
    function errorText(id) {
        const translated = qsTrId("synapse.monitor.error." + id)
        return translated.indexOf("synapse.monitor.error.") === 0
             ? qsTrId("synapse.monitor.error.generic") : translated
    }

    component NavigationButton: ItemDelegate {
        id: navigationButton
        required property string viewId
        required property string viewName
        required property string mark
        property bool horizontalMode: false
        width: horizontalMode ? Math.max(116, implicitWidth) : ListView.view.width
        height: horizontalMode ? 42 : 48
        padding: 0
        leftPadding: 0
        rightPadding: 0
        onClicked: monitorAdapter.selectView(navigationButton.viewId)
        Accessible.name: navigationButton.viewName
        background: Rectangle {
            radius: 10
            color: navigationButton.viewId === monitorAdapter.currentView
                   ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.18)
                   : (navigationButton.hovered ? window.hoverColor : "transparent")
            border.color: navigationButton.viewId === monitorAdapter.currentView
                          ? Qt.rgba(window.accentColor.r, window.accentColor.g, window.accentColor.b, 0.45)
                          : "transparent"
        }
        contentItem: RowLayout {
            spacing: 10
            Rectangle {
                Layout.preferredWidth: 26
                Layout.preferredHeight: 26
                radius: 8
                color: navigationButton.viewId === monitorAdapter.currentView
                       ? window.accentColor : window.surfaceColor
                Label {
                    anchors.centerIn: parent
                    text: navigationButton.mark
                    color: navigationButton.viewId === monitorAdapter.currentView
                           ? window.backgroundColor : window.textMuted
                    font.pixelSize: 12
                    font.weight: Font.Bold
                }
            }
            Label {
                Layout.fillWidth: true
                text: navigationButton.viewName
                color: navigationButton.viewId === monitorAdapter.currentView
                       ? window.textPrimary : window.textMuted
                font.pixelSize: 12
                font.weight: navigationButton.viewId === monitorAdapter.currentView
                             ? Font.DemiBold : Font.Normal
                elide: Text.ElideRight
            }
        }
    }

    Shortcut { sequence: "1"; onActivated: if (monitorAdapter.views.length > 0) monitorAdapter.selectView(monitorAdapter.views[0].id) }
    Shortcut { sequence: "2"; onActivated: if (monitorAdapter.views.length > 1) monitorAdapter.selectView(monitorAdapter.views[1].id) }
    Shortcut { sequence: "3"; onActivated: if (monitorAdapter.views.length > 2) monitorAdapter.selectView(monitorAdapter.views[2].id) }
    Shortcut { sequence: "4"; onActivated: if (monitorAdapter.views.length > 3) monitorAdapter.selectView(monitorAdapter.views[3].id) }
    Shortcut { sequence: "5"; onActivated: if (monitorAdapter.views.length > 4) monitorAdapter.selectView(monitorAdapter.views[4].id) }
    Shortcut { sequence: "6"; onActivated: if (monitorAdapter.views.length > 5) monitorAdapter.selectView(monitorAdapter.views[5].id) }

    Rectangle {
        anchors.fill: parent
        color: window.backgroundColor
        gradient: Gradient {
            GradientStop { position: 0; color: "#1a1b26" }
            GradientStop { position: 1; color: "#161720" }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 64
            color: window.sidebarColor
            border.color: window.borderColor
            border.width: 0

            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 18
                anchors.rightMargin: 18
                spacing: 12

                Rectangle {
                    Layout.preferredWidth: 34
                    Layout.preferredHeight: 34
                    radius: 11
                    color: window.accentColor
                    Label {
                        anchors.centerIn: parent
                        text: "M"
                        color: window.backgroundColor
                        font.pixelSize: 17
                        font.weight: Font.Bold
                    }
                }
                ColumnLayout {
                    spacing: 0
                    Label { text: qsTrId("synapse.monitor.title"); color: window.textPrimary; font.pixelSize: 17; font.weight: Font.DemiBold }
                    Label {
                        text: qsTrId("synapse.monitor.subtitle")
                        color: window.textMuted
                        font.pixelSize: 10
                        visible: !window.compact
                    }
                }
                Item { Layout.fillWidth: true }

                Rectangle {
                    implicitWidth: liveRow.implicitWidth + 18
                    implicitHeight: 30
                    radius: 15
                    color: monitorAdapter.ready
                           ? Qt.rgba(window.greenColor.r, window.greenColor.g, window.greenColor.b, 0.12)
                           : Qt.rgba(window.yellowColor.r, window.yellowColor.g, window.yellowColor.b, 0.12)
                    border.color: monitorAdapter.ready
                                  ? Qt.rgba(window.greenColor.r, window.greenColor.g, window.greenColor.b, 0.42)
                                  : Qt.rgba(window.yellowColor.r, window.yellowColor.g, window.yellowColor.b, 0.42)
                    Row {
                        id: liveRow
                        anchors.centerIn: parent
                        spacing: 7
                        Rectangle {
                            width: 7; height: 7; radius: 4
                            color: monitorAdapter.ready ? window.greenColor : window.yellowColor
                        }
                        Label {
                            text: monitorAdapter.ready ? qsTrId("synapse.monitor.status.live")
                                  : qsTrId("synapse.monitor.status.connecting")
                            color: monitorAdapter.ready ? window.greenColor : window.yellowColor
                            font.pixelSize: 10
                            font.weight: Font.DemiBold
                        }
                    }
                }

                SynapseComboBox {
                    id: intervalBox
                    model: [500, 1000, 2000, 5000]
                    currentIndex: Math.max(0, model.indexOf(monitorAdapter.intervalMilliseconds))
                    textForValue: function(value) { return String(value) + " ms" }
                    onActivated: monitorAdapter.setIntervalMilliseconds(Number(currentValue))
                    implicitWidth: 102
                    surfaceColor: window.surfaceColor; hoverColor: window.hoverColor
                    borderColor: window.borderColor; textColor: window.textPrimary
                    mutedColor: window.textMuted; accentColor: window.accentColor
                    Accessible.name: qsTrId("synapse.monitor.action.interval")
                }
                Label {
                    text: qsTrId("synapse.monitor.action.language") + ":"
                    color: window.textMuted
                    font.pixelSize: 11
                    font.weight: Font.DemiBold
                }
                SynapseComboBox {
                    model: ["it_IT", "en_US"]
                    currentIndex: Math.max(0, model.indexOf(monitorLocalization.currentLocale))
                    textForValue: function(value) {
                        return value === "it_IT" ? "Italiano" : "English"
                    }
                    onActivated: monitorLocalization.setLocale(String(currentValue))
                    implicitWidth: 112
                    surfaceColor: window.surfaceColor; hoverColor: window.hoverColor
                    borderColor: window.borderColor; textColor: window.textPrimary
                    mutedColor: window.textMuted; accentColor: window.accentColor
                    Accessible.name: qsTrId("synapse.monitor.action.language")
                }
            }
            Rectangle { anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom; height: 1; color: window.borderColor }
        }

        ListView {
            Layout.fillWidth: true
            Layout.preferredHeight: window.compact ? 52 : 0
            visible: window.compact
            orientation: ListView.Horizontal
            spacing: 6
            leftMargin: 12
            rightMargin: 12
            topMargin: 5
            bottomMargin: 5
            clip: true
            model: monitorAdapter.views
            delegate: NavigationButton {
                required property var modelData
                viewId: modelData.id
                viewName: window.viewLabel(modelData.id)
                mark: window.viewMark(modelData.id)
                horizontalMode: true
            }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0

            Rectangle {
                Layout.preferredWidth: window.compact ? 0 : 206
                Layout.fillHeight: true
                visible: !window.compact
                color: window.sidebarColor
                border.color: window.borderColor

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 8

                    Label {
                        Layout.leftMargin: 10
                        text: qsTrId("synapse.monitor.navigation.views")
                        color: window.textMuted
                        font.pixelSize: 10
                        font.weight: Font.DemiBold
                    }
                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 5
                        clip: true
                        model: monitorAdapter.views
                        delegate: NavigationButton {
                            required property var modelData
                            viewId: modelData.id
                            viewName: window.viewLabel(modelData.id)
                            mark: window.viewMark(modelData.id)
                        }
                    }
                    Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: window.borderColor }
                    Label {
                        Layout.fillWidth: true
                        Layout.margins: 8
                        text: qsTrId("synapse.monitor.readonly.local")
                        color: window.textMuted
                        font.pixelSize: 10
                        wrapMode: Text.WordWrap
                    }
                }
            }

            Item {
                Layout.fillWidth: true
                Layout.fillHeight: true

                Loader {
                    id: viewLoader
                    anchors.fill: parent
                    anchors.margins: window.compact ? 14 : 18
                    active: monitorAdapter.ready
                    sourceComponent: {
                        if (monitorAdapter.currentView === "processes") return processesComponent
                        if (monitorAdapter.currentView === "performance") return performanceComponent
                        if (monitorAdapter.currentView === "services"
                            || monitorAdapter.currentView === "startup"
                            || monitorAdapter.currentView === "connections") return inventoryComponent
                        return informationComponent
                    }
                }

                Column {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 48, 420)
                    spacing: 14
                    visible: !monitorAdapter.ready && monitorAdapter.errorId.length === 0
                    BusyIndicator { anchors.horizontalCenter: parent.horizontalCenter; running: parent.visible }
                    Label {
                        width: parent.width
                        text: qsTrId("synapse.monitor.status.loading")
                        color: window.textMuted
                        horizontalAlignment: Text.AlignHCenter
                        wrapMode: Text.WordWrap
                    }
                }

                Rectangle {
                    anchors.centerIn: parent
                    width: Math.min(parent.width - 48, 500)
                    implicitHeight: errorColumn.implicitHeight + 34
                    radius: 16
                    color: window.surfaceColor
                    border.color: window.redColor
                    visible: monitorAdapter.errorId.length > 0
                    ColumnLayout {
                        id: errorColumn
                        anchors.left: parent.left; anchors.right: parent.right; anchors.top: parent.top
                        anchors.margins: 17; spacing: 8
                        Label { text: qsTrId("synapse.monitor.error.title"); color: window.redColor; font.pixelSize: 17; font.weight: Font.DemiBold }
                        Label { Layout.fillWidth: true; text: window.errorText(monitorAdapter.errorId); color: window.textMuted; wrapMode: Text.WordWrap }
                        Label { Layout.fillWidth: true; text: qsTrId("synapse.monitor.error.safe-stop"); color: window.textMuted; font.pixelSize: 10; wrapMode: Text.WordWrap }
                    }
                }
            }
        }
    }

    Component {
        id: processesComponent
        ProcessesView {
            adapter: monitorAdapter
            backgroundColor: window.backgroundColor; surfaceColor: window.surfaceColor
            alternateColor: window.alternateColor; hoverColor: window.hoverColor
            borderColor: window.borderColor; textColor: window.textPrimary
            mutedColor: window.textMuted; accentColor: window.accentColor
            greenColor: window.greenColor; yellowColor: window.yellowColor; purpleColor: window.purpleColor
        }
    }
    Component {
        id: performanceComponent
        PerformanceView {
            adapter: monitorAdapter
            backgroundColor: window.backgroundColor; surfaceColor: window.surfaceColor
            alternateColor: window.alternateColor; borderColor: window.borderColor
            textColor: window.textPrimary; mutedColor: window.textMuted
            accentColor: window.accentColor; greenColor: window.greenColor
            yellowColor: window.yellowColor; redColor: window.redColor; purpleColor: window.purpleColor
        }
    }
    Component {
        id: inventoryComponent
        InventoryView {
            adapter: monitorAdapter
            viewId: monitorAdapter.currentView
            surfaceColor: window.surfaceColor; alternateColor: window.alternateColor
            hoverColor: window.hoverColor; borderColor: window.borderColor
            textColor: window.textPrimary; mutedColor: window.textMuted
            accentColor: window.accentColor
        }
    }
    Component {
        id: informationComponent
        InformationView {
            adapter: monitorAdapter
            surfaceColor: window.surfaceColor; borderColor: window.borderColor
            textColor: window.textPrimary; mutedColor: window.textMuted
            accentColor: window.accentColor
        }
    }
}
