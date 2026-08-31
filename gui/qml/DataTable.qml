// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root
    property var tableModel
    property var filterController: null
    property var columnDefinitions: []
    property var sortableIds: []
    property var activeFilterIds: []
    property string activeSortId: ""
    property bool sortAscending: true
    property color surfaceColor: "#24283b"
    property color alternateColor: "#202435"
    property color hoverColor: "#292e42"
    property color borderColor: "#414868"
    property color textColor: "#c0caf5"
    property color mutedColor: "#9aa5ce"
    property color accentColor: "#7aa2f7"
    property int rowHeight: 42
    readonly property alias instantiatedRowCount: rowList.instantiatedRows
    signal rowActivated(var record)
    signal sortRequested(string sortId)

    function columnSortId(definition) {
        return String(definition.sortId || definition.key || "")
    }

    function isSortable(definition) {
        return sortableIds.indexOf(columnSortId(definition)) >= 0
    }

    function isFiltered(definition) {
        return activeFilterIds.indexOf(String(definition.key || "")) >= 0
    }

    function requestSort(definition) {
        const identifier = columnSortId(definition)
        if (sortableIds.indexOf(identifier) >= 0)
            sortRequested(identifier)
    }

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

    Flickable {
        id: horizontalViewport
        anchors.fill: parent
        anchors.margins: 1
        clip: true
        contentWidth: root.totalWidth()
        contentHeight: height
        flickableDirection: Flickable.HorizontalFlick
        boundsBehavior: Flickable.StopAtBounds
        ScrollBar.horizontal: ScrollBar { policy: ScrollBar.AsNeeded }

        Column {
            width: root.totalWidth()
            height: horizontalViewport.height

            Rectangle {
                width: parent.width
                height: 40
                color: Qt.darker(root.surfaceColor, 1.08)
                border.color: root.borderColor
                border.width: 0

                Row {
                    anchors.fill: parent
                    Repeater {
                        model: root.columnDefinitions
                        delegate: Item {
                            id: headerCell
                            objectName: "column-header-" + String(modelData.key || "")
                            required property var modelData
                            property string sortId: root.columnSortId(headerCell.modelData)
                            property bool sortable: root.isSortable(headerCell.modelData)
                            property bool selected: headerCell.sortable
                                                    && root.activeSortId === headerCell.sortId
                            property bool filtered: root.isFiltered(headerCell.modelData)
                            property bool filterable: root.filterController !== null
                            width: Number(headerCell.modelData.width || 120)
                            height: parent.height
                            activeFocusOnTab: headerCell.sortable
                            Accessible.role: headerCell.sortable
                                             ? Accessible.Button : Accessible.StaticText
                            Accessible.name: String(headerCell.modelData.label
                                                    || headerCell.modelData.key)
                            Accessible.description: headerCell.sortable
                                                    ? qsTrId("synapse.monitor.accessibility.sort-column") : ""
                            Keys.onReturnPressed: root.requestSort(headerCell.modelData)
                            Keys.onSpacePressed: root.requestSort(headerCell.modelData)

                            Rectangle {
                                anchors.fill: parent
                                color: headerCell.selected
                                       ? Qt.rgba(root.accentColor.r, root.accentColor.g,
                                                 root.accentColor.b, 0.12)
                                       : (headerSortMouse.containsMouse
                                          ? root.hoverColor : "transparent")
                            }
                            Label {
                                anchors.left: parent.left
                                anchors.leftMargin: 12
                                anchors.right: sortIndicator.left
                                anchors.rightMargin: 5
                                anchors.verticalCenter: parent.verticalCenter
                                text: headerCell.modelData.label || headerCell.modelData.key
                                color: headerCell.selected ? root.accentColor : root.mutedColor
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                                elide: Text.ElideRight
                                horizontalAlignment: headerCell.modelData.align === "right"
                                                     ? Text.AlignRight : Text.AlignLeft
                            }
                            Label {
                                id: sortIndicator
                                anchors.right: filterButton.left
                                anchors.rightMargin: 2
                                anchors.verticalCenter: parent.verticalCenter
                                visible: headerCell.selected
                                width: headerCell.selected ? 12 : 0
                                text: root.sortAscending ? "↑" : "↓"
                                color: headerCell.selected ? root.accentColor : root.mutedColor
                                font.pixelSize: 11
                                font.weight: Font.DemiBold
                            }
                            Item {
                                id: filterButton
                                objectName: "column-filter-" + String(headerCell.modelData.key || "")
                                property var controlledPopup: filterPopup
                                anchors.right: parent.right
                                anchors.rightMargin: 3
                                anchors.verticalCenter: parent.verticalCenter
                                width: headerCell.filterable ? 26 : 0
                                height: 30
                                visible: headerCell.filterable
                                activeFocusOnTab: true
                                Accessible.role: Accessible.Button
                                Accessible.name: qsTrId("synapse.monitor.filter.column") + " "
                                                 + String(headerCell.modelData.label
                                                          || headerCell.modelData.key)
                                Keys.onReturnPressed: filterPopup.openForColumn()
                                Keys.onSpacePressed: filterPopup.openForColumn()
                                Rectangle {
                                    anchors.fill: parent
                                    radius: 7
                                    color: headerCell.filtered
                                           ? Qt.rgba(root.accentColor.r, root.accentColor.g,
                                                     root.accentColor.b, 0.2)
                                           : (filterMouse.containsMouse
                                              ? root.hoverColor : "transparent")
                                    border.color: headerCell.filtered
                                                  ? root.accentColor : "transparent"
                                }
                                Label {
                                    anchors.centerIn: parent
                                    text: headerCell.filtered ? "●" : "⌄"
                                    color: headerCell.filtered ? root.accentColor : root.mutedColor
                                    font.pixelSize: headerCell.filtered ? 8 : 13
                                }
                                MouseArea {
                                    id: filterMouse
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: filterPopup.openForColumn()
                                }
                            }
                            Rectangle {
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.bottom: parent.bottom
                                height: headerCell.selected || headerCell.filtered ? 2 : 1
                                color: headerCell.selected || headerCell.filtered
                                       ? root.accentColor : root.borderColor
                            }
                            MouseArea {
                                id: headerSortMouse
                                anchors.left: parent.left
                                anchors.top: parent.top
                                anchors.bottom: parent.bottom
                                anchors.right: filterButton.left
                                enabled: headerCell.sortable
                                hoverEnabled: headerCell.sortable
                                cursorShape: Qt.PointingHandCursor
                                onClicked: root.requestSort(headerCell.modelData)
                            }

                            Popup {
                                id: filterPopup
                                objectName: "column-filter-popup-" + String(headerCell.modelData.key || "")
                                width: 300
                                height: 370
                                y: headerCell.height + 3
                                margins: 12
                                padding: 12
                                modal: false
                                focus: true
                                closePolicy: Popup.CloseOnEscape
                                             | Popup.CloseOnPressOutside
                                property var options: []
                                property var selectedTokens: []
                                property string searchText: ""

                                function openForColumn() {
                                    if (!root.filterController) return
                                    options = root.filterController.columnFilterOptions(
                                                  String(headerCell.modelData.key))
                                    const alreadyFiltered = root.filterController.columnFilterActive(
                                                                String(headerCell.modelData.key))
                                    selectedTokens = alreadyFiltered
                                        ? root.filterController.activeColumnFilterTokens(
                                              String(headerCell.modelData.key))
                                        : options.map(function(option) { return String(option.token) })
                                    searchText = ""
                                    optionSearch.text = ""
                                    open()
                                }

                                function filteredOptions() {
                                    const needle = searchText.toLocaleLowerCase()
                                    if (!needle.length) return options
                                    return options.filter(function(option) {
                                        const displayed = root.present(
                                            option.value,
                                            String(headerCell.modelData.format || "text"))
                                        return displayed.toLocaleLowerCase().indexOf(needle) >= 0
                                    })
                                }

                                function setSelected(token, checked) {
                                    const copy = selectedTokens.slice()
                                    const index = copy.indexOf(token)
                                    if (checked && index < 0) copy.push(token)
                                    else if (!checked && index >= 0) copy.splice(index, 1)
                                    selectedTokens = copy
                                }

                                background: Rectangle {
                                    radius: 12
                                    color: root.surfaceColor
                                    border.color: root.borderColor
                                }

                                contentItem: ColumnLayout {
                                    spacing: 8
                                    Label {
                                        Layout.fillWidth: true
                                        text: qsTrId("synapse.monitor.filter.title") + " · "
                                              + String(headerCell.modelData.label
                                                       || headerCell.modelData.key)
                                        color: root.textColor
                                        font.pixelSize: 13
                                        font.weight: Font.DemiBold
                                        elide: Text.ElideRight
                                    }
                                    TextField {
                                        id: optionSearch
                                        objectName: "column-filter-search-" + String(headerCell.modelData.key || "")
                                        Layout.fillWidth: true
                                        placeholderText: qsTrId("synapse.monitor.filter.search")
                                        maximumLength: 64
                                        color: root.textColor
                                        placeholderTextColor: root.mutedColor
                                        selectByMouse: true
                                        onTextEdited: filterPopup.searchText = text
                                        background: Rectangle {
                                            radius: 8
                                            color: root.alternateColor
                                            border.color: optionSearch.activeFocus
                                                          ? root.accentColor : root.borderColor
                                        }
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 6
                                        Button {
                                            text: qsTrId("synapse.monitor.filter.select-all")
                                            flat: true
                                            onClicked: filterPopup.selectedTokens
                                                       = filterPopup.options.map(
                                                           function(option) {
                                                               return String(option.token)
                                                           })
                                        }
                                        Button {
                                            text: qsTrId("synapse.monitor.filter.select-none")
                                            flat: true
                                            onClicked: filterPopup.selectedTokens = []
                                        }
                                        Item { Layout.fillWidth: true }
                                        Label {
                                            text: filterPopup.selectedTokens.length + " / "
                                                  + filterPopup.options.length
                                            color: root.mutedColor
                                            font.pixelSize: 10
                                        }
                                    }
                                    ListView {
                                        id: optionList
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        clip: true
                                        spacing: 2
                                        model: filterPopup.filteredOptions()
                                        ScrollIndicator.vertical: ScrollIndicator { }
                                        delegate: CheckDelegate {
                                            id: optionDelegate
                                            required property var modelData
                                            width: optionList.width
                                            height: 34
                                            checked: filterPopup.selectedTokens.indexOf(
                                                         String(optionDelegate.modelData.token)) >= 0
                                            onToggled: filterPopup.setSelected(
                                                           String(optionDelegate.modelData.token),
                                                           checked)
                                            contentItem: RowLayout {
                                                Label {
                                                    Layout.fillWidth: true
                                                    text: root.present(
                                                        optionDelegate.modelData.value,
                                                        String(headerCell.modelData.format || "text"))
                                                    color: root.textColor
                                                    elide: Text.ElideRight
                                                    font.pixelSize: 11
                                                }
                                                Label {
                                                    text: String(optionDelegate.modelData.count)
                                                    color: root.mutedColor
                                                    font.pixelSize: 10
                                                }
                                            }
                                            background: Rectangle {
                                                radius: 7
                                                color: optionDelegate.hovered
                                                       ? root.hoverColor : "transparent"
                                            }
                                        }
                                    }
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 7
                                        Button {
                                            objectName: "column-filter-clear-" + String(headerCell.modelData.key || "")
                                            text: qsTrId("synapse.monitor.filter.clear")
                                            onClicked: {
                                                if (root.filterController.clearColumnFilter(
                                                        String(headerCell.modelData.key)))
                                                    filterPopup.close()
                                            }
                                        }
                                        Item { Layout.fillWidth: true }
                                        Button {
                                            objectName: "column-filter-apply-" + String(headerCell.modelData.key || "")
                                            text: qsTrId("synapse.monitor.filter.apply")
                                            highlighted: true
                                            onClicked: {
                                                if (root.filterController.setColumnFilter(
                                                        String(headerCell.modelData.key),
                                                        filterPopup.selectedTokens, true))
                                                    filterPopup.close()
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            ListView {
                id: rowList
                property int instantiatedRows: 0
                objectName: "virtualized-row-list"
                width: parent.width
                height: Math.max(0, parent.height - 40)
                clip: true
                model: root.tableModel
                reuseItems: true
                cacheBuffer: root.rowHeight * 2
                boundsBehavior: Flickable.StopAtBounds
                flickableDirection: Flickable.VerticalFlick
                delegate: Rectangle {
                    id: rowDelegate
                    required property var row
                    required property int index
                    property var record: row
                    width: rowList.width
                    height: root.rowHeight
                    Component.onCompleted: rowList.instantiatedRows += 1
                    Component.onDestruction: rowList.instantiatedRows -= 1
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

    ScrollBar {
        id: verticalScrollBar
        anchors.top: root.top
        anchors.topMargin: 41
        anchors.right: root.right
        anchors.bottom: root.bottom
        orientation: Qt.Vertical
        size: Math.min(1, rowList.height / Math.max(1, rowList.contentHeight))
        position: rowList.contentY / Math.max(1, rowList.contentHeight)
        active: rowList.movingVertically || pressed
        visible: size < 1
        onPositionChanged: {
            if (pressed)
                rowList.contentY = position * rowList.contentHeight
        }
    }
}
