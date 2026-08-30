// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property var tableModel
    property var columnDefinitions: []
    property color surfaceColor: "#24283b"
    property color alternateColor: "#202435"
    property color hoverColor: "#292e42"
    property color borderColor: "#414868"
    property color textColor: "#c0caf5"
    property color mutedColor: "#9aa5ce"
    property color accentColor: "#7aa2f7"
    property int rowHeight: 42
    signal rowActivated(var record)

    function totalWidth() {
        let result = 0
        for (let index = 0; index < columnDefinitions.length; ++index)
            result += Number(columnDefinitions[index].width || 120)
        return Math.max(width - 2, result)
    }

    function present(value, kind) {
        if (value === null || value === undefined)
            return "—"
        const number = Number(value)
        if (kind === "percent")
            return isFinite(number) ? (number / 1000).toFixed(1) + "%" : "—"
        if (kind === "bytes") {
            if (!isFinite(number)) return "—"
            const units = ["B", "KiB", "MiB", "GiB", "TiB"]
            let scaled = Math.max(0, number)
            let unit = 0
            while (scaled >= 1024 && unit < units.length - 1) {
                scaled /= 1024
                ++unit
            }
            return (unit === 0 ? scaled.toFixed(0) : scaled.toFixed(1)) + " " + units[unit]
        }
        if (kind === "rate")
            return present(number, "bytes") + "/s"
        if (kind === "pid")
            return isFinite(number) && number > 0 ? Math.trunc(number).toString() : "—"
        if (kind === "integer")
            return isFinite(number) ? Math.trunc(number).toString() : "—"
        return String(value)
    }

    radius: 14
    color: surfaceColor
    border.color: borderColor
    border.width: 1
    clip: true

    ScrollView {
        id: scroll
        anchors.fill: parent
        anchors.margins: 1
        clip: true
        ScrollBar.horizontal.policy: ScrollBar.AsNeeded
        ScrollBar.vertical.policy: ScrollBar.AsNeeded

        Column {
            width: root.totalWidth()

            Rectangle {
                width: parent.width
                height: 38
                color: Qt.darker(root.surfaceColor, 1.08)
                border.color: root.borderColor
                border.width: 0

                Row {
                    anchors.fill: parent
                    Repeater {
                        model: root.columnDefinitions
                        delegate: Item {
                            id: headerCell
                            required property var modelData
                            width: Number(headerCell.modelData.width || 120)
                            height: parent.height
                            Label {
                                anchors.left: parent.left
                                anchors.leftMargin: 12
                                anchors.right: parent.right
                                anchors.rightMargin: 8
                                anchors.verticalCenter: parent.verticalCenter
                                text: headerCell.modelData.label || headerCell.modelData.key
                                color: root.mutedColor
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                horizontalAlignment: headerCell.modelData.align === "right"
                                                     ? Text.AlignRight : Text.AlignLeft
                            }
                        }
                    }
                }
            }

            Repeater {
                model: root.tableModel
                delegate: Rectangle {
                    id: rowDelegate
                    required property var row
                    required property int index
                    property var record: row
                    width: root.totalWidth()
                    height: root.rowHeight
                    color: rowMouse.containsMouse ? root.hoverColor
                          : (index % 2 ? root.alternateColor : root.surfaceColor)

                    Row {
                        anchors.fill: parent
                        Repeater {
                            model: root.columnDefinitions
                            delegate: Item {
                                id: dataCell
                                required property var modelData
                                width: Number(dataCell.modelData.width || 120)
                                height: rowDelegate.height
                                Label {
                                    anchors.left: parent.left
                                    anchors.leftMargin: 12
                                    anchors.right: parent.right
                                    anchors.rightMargin: 8
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: root.present(rowDelegate.record[dataCell.modelData.key],
                                                       dataCell.modelData.format || "text")
                                    color: dataCell.modelData.key === "name"
                                           || dataCell.modelData.key === "description"
                                           ? root.textColor : root.mutedColor
                                    font.pixelSize: 12
                                    elide: Text.ElideRight
                                    horizontalAlignment: dataCell.modelData.align === "right"
                                                         ? Text.AlignRight : Text.AlignLeft
                                }
                            }
                        }
                    }
                    Rectangle {
                        anchors.left: parent.left
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        height: 1
                        color: Qt.rgba(root.borderColor.r, root.borderColor.g,
                                       root.borderColor.b, 0.45)
                    }
                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.rowActivated(rowDelegate.record)
                    }
                }
            }
        }
    }
}
