// SPDX-License-Identifier: MIT
pragma ComponentBehavior: Bound
import QtQuick

Canvas {
    id: root
    property var values: []
    property color lineColor: "#7aa2f7"
    property color gridColor: "#414868"
    property real maximum: 100000
    property real minimum: 0

    onValuesChanged: requestPaint()
    onWidthChanged: requestPaint()
    onHeightChanged: requestPaint()
    onLineColorChanged: requestPaint()

    onPaint: {
        const context = getContext("2d")
        context.reset()
        context.clearRect(0, 0, width, height)
        context.strokeStyle = gridColor
        context.globalAlpha = 0.35
        context.lineWidth = 1
        for (let index = 1; index < 4; ++index) {
            const y = Math.round(height * index / 4) + 0.5
            context.beginPath()
            context.moveTo(0, y)
            context.lineTo(width, y)
            context.stroke()
        }
        context.globalAlpha = 1
        if (!values || values.length < 1)
            return
        const span = Math.max(1, maximum - minimum)
        const denominator = Math.max(1, values.length - 1)
        context.strokeStyle = lineColor
        context.lineWidth = 2
        context.lineJoin = "round"
        context.lineCap = "round"
        let drawing = false
        for (let index = 0; index < values.length; ++index) {
            const value = values[index]
            if (value === null || value === undefined || !isFinite(Number(value))) {
                if (drawing) context.stroke()
                drawing = false
                continue
            }
            const x = index * width / denominator
            const normalized = Math.max(0, Math.min(1, (Number(value) - minimum) / span))
            const y = height - normalized * Math.max(1, height - 2) - 1
            if (!drawing) {
                context.beginPath()
                context.moveTo(x, y)
                drawing = true
            } else {
                context.lineTo(x, y)
            }
        }
        if (drawing) context.stroke()
    }
}
