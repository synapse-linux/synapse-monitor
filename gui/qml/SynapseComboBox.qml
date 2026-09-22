// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Controls

ComboBox {
    id: control
    property color surfaceColor: "#24283b"
    property color hoverColor: "#292e42"
    property color borderColor: "#414868"
    property color textColor: "#c0caf5"
    property color mutedColor: "#9aa5ce"
    property color accentColor: "#7aa2f7"
    property var textForValue: function(value) { return String(value) }

    leftPadding: 11
    rightPadding: 30
    topPadding: 6
    bottomPadding: 6
    implicitHeight: 32
    displayText: textForValue(currentValue)

    delegate: ItemDelegate {
        id: option
        required property var modelData
        required property int index
        width: control.width - 8
        height: 34
        leftPadding: 10
        highlighted: control.highlightedIndex === option.index
        contentItem: Label {
            text: control.textForValue(option.modelData)
            color: option.highlighted ? control.textColor : control.mutedColor
            font.pixelSize: 11
            verticalAlignment: Text.AlignVCenter
            elide: Text.ElideRight
        }
        background: Rectangle {
            radius: 7
            color: option.highlighted ? control.hoverColor : "transparent"
        }
    }

    indicator: Label {
        x: control.width - width - 10
        y: Math.round((control.height - height) / 2) - 1
        text: "⌄"
        color: control.mutedColor
        font.pixelSize: 14
    }

    contentItem: Label {
        leftPadding: 0
        rightPadding: 0
        text: control.displayText
        color: control.textColor
        font.pixelSize: 11
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }

    background: Rectangle {
        radius: 8
        color: control.down ? control.hoverColor : control.surfaceColor
        border.color: control.activeFocus ? control.accentColor : control.borderColor
        border.width: 1
    }

    popup: Popup {
        y: control.height + 4
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 250)
        padding: 4
        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null
            currentIndex: control.highlightedIndex
            ScrollIndicator.vertical: ScrollIndicator { }
        }
        background: Rectangle {
            radius: 9
            color: control.surfaceColor
            border.color: control.borderColor
        }
    }
}
