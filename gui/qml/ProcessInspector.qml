// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property var payload: ({})
    property bool busy: false
    property string errorId: ""
    property color surfaceColor: "#24283b"
    property color backgroundColor: "#1a1b26"
    property color borderColor: "#414868"
    property color textColor: "#c0caf5"
    property color mutedColor: "#9aa5ce"
    property color accentColor: "#7aa2f7"
    signal closeRequested()

    function present(value) {
        return value === null || value === undefined || value === "" ? "—" : String(value)
    }

    radius: 16
    color: surfaceColor
    border.color: borderColor
    border.width: 1

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 16
        spacing: 12

        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: qsTrId("synapse.monitor.inspector.title")
                color: root.textColor
                font.pixelSize: 16
                font.weight: Font.DemiBold
            }
            ToolButton {
                id: closeButton
                text: "×"
                onClicked: root.closeRequested()
                Accessible.name: qsTrId("synapse.monitor.action.close")
                contentItem: Label {
                    text: closeButton.text
                    color: root.mutedColor
                    font.pixelSize: 20
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                background: Rectangle { color: "transparent"; radius: 8 }
            }
        }

        BusyIndicator {
            Layout.alignment: Qt.AlignHCenter
            running: root.busy
            visible: running
        }

        Label {
            Layout.fillWidth: true
            visible: root.errorId.length > 0
            text: qsTrId("synapse.monitor.error." + root.errorId)
            color: "#f7768e"
            wrapMode: Text.WordWrap
        }

        ScrollView {
            Layout.fillWidth: true
            Layout.fillHeight: true
            visible: !root.busy && root.errorId.length === 0
            clip: true

            ColumnLayout {
                id: details
                width: Math.max(260, parent.width)
                spacing: 12
                property var identity: root.payload.identity || ({})
                property var credentials: root.payload.credentials || ({})
                property var descriptors: root.payload.descriptors || ({})
                property var modules: root.payload.modules || ({})
                property var capabilities: credentials.capabilities || ({})

                Label {
                    Layout.fillWidth: true
                    text: details.identity.name || "—"
                    color: root.textColor
                    font.pixelSize: 20
                    font.weight: Font.DemiBold
                    elide: Text.ElideRight
                }
                Label {
                    Layout.fillWidth: true
                    text: "PID " + root.present(details.identity.pid)
                          + "  ·  " + qsTrId("synapse.monitor.inspector.start")
                          + " " + root.present(details.identity.startTicks)
                    color: root.mutedColor
                    font.pixelSize: 11
                    wrapMode: Text.WrapAnywhere
                }

                Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: root.borderColor }

                Label {
                    text: qsTrId("synapse.monitor.inspector.security")
                    color: root.accentColor
                    font.weight: Font.DemiBold
                }
                Label {
                    Layout.fillWidth: true
                    text: "UID  " + root.present(details.credentials.uids)
                          + "\nGID  " + root.present(details.credentials.gids)
                          + "\nNoNewPrivileges  " + root.present(details.credentials.noNewPrivileges)
                          + "\nSeccomp  " + root.present(details.credentials.seccompMode)
                    color: root.mutedColor
                    font.pixelSize: 11
                    wrapMode: Text.WrapAnywhere
                }

                Label {
                    text: qsTrId("synapse.monitor.inspector.capabilities")
                    color: root.accentColor
                    font.weight: Font.DemiBold
                }
                Label {
                    Layout.fillWidth: true
                    text: "Eff  " + root.present(details.capabilities.effective)
                          + "\nPrm  " + root.present(details.capabilities.permitted)
                          + "\nBnd  " + root.present(details.capabilities.bounding)
                    color: root.mutedColor
                    font.pixelSize: 11
                    wrapMode: Text.WrapAnywhere
                }

                Label {
                    text: qsTrId("synapse.monitor.inspector.descriptors")
                    color: root.accentColor
                    font.weight: Font.DemiBold
                }
                Label {
                    Layout.fillWidth: true
                    text: root.present(details.descriptors.count) + "  ·  "
                          + root.present(details.descriptors.socketCount) + " "
                          + qsTrId("synapse.monitor.inspector.sockets")
                    color: root.mutedColor
                }

                Label {
                    text: qsTrId("synapse.monitor.inspector.modules")
                    color: root.accentColor
                    font.weight: Font.DemiBold
                }
                Repeater {
                    model: details.modules.names || []
                    delegate: Label {
                        required property var modelData
                        Layout.fillWidth: true
                        text: modelData
                        color: root.mutedColor
                        font.pixelSize: 11
                        elide: Text.ElideMiddle
                    }
                }
            }
        }

        Label {
            Layout.fillWidth: true
            text: qsTrId("synapse.monitor.readonly.detail")
            color: root.mutedColor
            font.pixelSize: 10
            wrapMode: Text.WordWrap
        }
    }
}
