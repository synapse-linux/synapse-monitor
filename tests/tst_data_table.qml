// SPDX-License-Identifier: MIT
import QtQuick
import QtQuick.Controls
import QtTest
import "../gui/qml"

TestCase {
    id: testCase
    name: "DataTableInteractions"
    when: windowShown

    property string tokenA: "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
    property string tokenB: "bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb"

    QtObject {
        id: controller
        property string appliedColumn: ""
        property var appliedTokens: []
        property bool cleared: false
        function columnFilterOptions(column) {
            return [
                { token: testCase.tokenA, value: "alpha", unavailable: false, count: 2 },
                { token: testCase.tokenB, value: null, unavailable: true, count: 1 }
            ]
        }
        function columnFilterActive(column) { return false }
        function activeColumnFilterTokens(column) { return [] }
        function setColumnFilter(column, tokens, enabled) {
            appliedColumn = column
            appliedTokens = tokens.slice()
            return enabled
        }
        function clearColumnFilter(column) {
            cleared = column === "name"
            return cleared
        }
    }

    ApplicationWindow {
        id: window
        width: 640
        height: 420
        visible: true
        color: "#1a1b26"

        DataTable {
            id: table
            objectName: "test-data-table"
            anchors.fill: parent
            anchors.margins: 20
            tableModel: null
            filterController: controller
            columnDefinitions: [
                { key: "name", sortId: "name", label: "Name", width: 220 }
            ]
            sortableIds: ["name"]
            activeSortId: "name"
            sortAscending: true
            activeFilterIds: []
        }
    }

    ListModel {
        id: largeRows
        dynamicRoles: true
    }

    SignalSpy {
        id: sortSpy
        target: table
        signalName: "sortRequested"
    }

    function test_sortHeaderAndFilterPopup() {
        const header = findChild(table, "column-header-name")
        verify(header !== null)
        mouseClick(header, 30, 20, Qt.LeftButton)
        compare(sortSpy.count, 1)
        compare(sortSpy.signalArguments[0][0], "name")

        const filterButton = findChild(table, "column-filter-name")
        verify(filterButton !== null)
        mouseClick(filterButton, filterButton.width / 2,
                   filterButton.height / 2, Qt.LeftButton)
        const popup = filterButton.controlledPopup
        verify(popup !== null)
        tryCompare(popup, "visible", true)
        compare(popup.options.length, 2)
        compare(popup.selectedTokens.length, 2)
        popup.selectedTokens = [tokenA]
        const apply = findChild(popup, "column-filter-apply-name")
        verify(apply !== null)
        apply.clicked()
        tryCompare(popup, "visible", false)
        compare(controller.appliedColumn, "name")
        compare(controller.appliedTokens.length, 1)
        compare(controller.appliedTokens[0], tokenA)
    }

    function test_virtualizesBoundedRealRowCohort() {
        largeRows.clear()
        for (let index = 0; index < 512; ++index) {
            largeRows.append({
                row: { name: "process-" + index },
                stableIdentity: "pid/" + index
            })
        }
        table.tableModel = largeRows
        tryVerify(function() { return table.instantiatedRowCount > 0 })
        verify(table.instantiatedRowCount <= 20,
               "viewport created " + table.instantiatedRowCount + " row delegates")
        compare(largeRows.count, 512)
        table.tableModel = null
        tryCompare(table, "instantiatedRowCount", 0)
    }
}
