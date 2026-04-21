import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

Window {
    visible: true
    width: 800
    height: 480
    color: '#ee1c1c'
    title: "Tufts Electric Racing"

    // Rotated container for landscape orientation
    Item {
        width: parent.height
        height: parent.width
        anchors.centerIn: parent
        rotation: -90
        transformOrigin: Item.Center

        // Background gradient
        Rectangle {
            anchors.fill: parent
            gradient: Gradient {
                GradientStop { position: 0.0; color: "#1a1a1a" }
                GradientStop { position: 1.0; color: "#000000" }
            }
        }

        // Main layout
        Item {
            anchors.fill: parent
            anchors.margins: 0

            // ── Fault Banner ─────────────────────────────────────────
            Rectangle {
                id: faultBanner
                visible: backend.faultActive
                anchors.top: parent.top
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.topMargin: 8
                width: parent.width * 0.7
                height: 40
                color: "#e74c3c"
                radius: 6
                z: 10

                Row {
                    anchors.centerIn: parent
                    spacing: 10
                    Text {
                        text: "⚠ FAULT"
                        color: "white"
                        font.pixelSize: 16
                        font.bold: true
                    }
                    Text {
                        text: backend.faultCode
                        color: "white"
                        font.pixelSize: 13
                    }
                }
            }

            // ── Top: Battery SOC Gauge ────────────────────────────────
            CircularGauge {
                id: batteryGauge
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 40
                width: 190
                height: 190

                minValue: 0
                maxValue: 100
                value: backend.batteryPercent
                unit: "%"
                label: "BATTERY"
                gaugeColor: backend.batteryPercent > 20 ? "#2ecc71" : "#e74c3c"
            }

            // ── Left: Speed Gauge ─────────────────────────────────────
            CircularGauge {
                id: speedGauge
                anchors.left: parent.left
                anchors.leftMargin: 5
                anchors.verticalCenter: parent.verticalCenter
                width: 190
                height: 190

                minValue: 0
                maxValue: 60
                value: backend.speed
                unit: "MPH"
                label: "SPEED"
                gaugeColor: backend.speed > 50 ? "#e74c3c" : "#3498db"
            }

            // ── Right: RPM Gauge ──────────────────────────────────────
            CircularGauge {
                id: rpmGauge
                anchors.right: parent.right
                anchors.rightMargin: 5
                anchors.verticalCenter: parent.verticalCenter
                width: 190
                height: 190

                minValue: 0
                maxValue: 6000
                value: backend.rpm
                unit: "RPM"
                label: "MOTOR"
                gaugeColor: backend.rpm > 5000 ? "#e74c3c" : "#f39c12"
            }

            // ── Middle row: Mileage + Pack Voltage ───────────────────
            Row {
                id: bottomStats
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: bottomButtons.top
                anchors.bottomMargin: 15
                spacing: 20

                Rectangle {
                    width: 180; height: 80
                    color: "#2c3e50"; radius: 10
                    border.color: '#f0f2f0'; border.width: 2
                    Column {
                        anchors.centerIn: parent; spacing: 5
                        Text {
                            text: "MILEAGE"
                            color: "#ecf0f1"; font.pixelSize: 14; font.bold: true
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                        Text {
                            text: backend.mileage.toFixed(2) + " mi"
                            color: "#ecf0f1"; font.pixelSize: 28; font.bold: true
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }
                }

                Rectangle {
                    width: 180; height: 80
                    color: "#2c3e50"; radius: 10
                    border.color: "#f0f2f0"; border.width: 2
                    Column {
                        anchors.centerIn: parent; spacing: 5
                        Text {
                            text: "PACK VOLTAGE"
                            color: "#ecf0f1"; font.pixelSize: 14; font.bold: true
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                        Text {
                            text: backend.voltage.toFixed(1) + " V"
                            color: "#ecf0f1"; font.pixelSize: 28; font.bold: true
                            anchors.horizontalCenter: parent.horizontalCenter
                        }
                    }
                }
            }

            // ── Bottom row: BATT TEMP + MOTOR TEMP ────────────────────
            Row {
                id: bottomButtons
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 20
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 20

                // ── Left button: Battery (cell) temperature from 0xC1 ──
                InfoButton {
                    width: 180
                    height: 80
                    title: "BATT TEMP"
                    value: backend.batteryTemp.toFixed(1) + "°C"
                    subtext: backend.batteryTemp > 45 ? "⚠ HOT" : "OK"
                    buttonColor: backend.batteryTemp > 45 ? "#e74c3c" : "#27ae60"
                }

                // ── Right button: Motor controller temperature ──────────
                InfoButton {
                    width: 180
                    height: 80
                    title: "MOTOR TEMP"
                    value: backend.temp.toFixed(1) + "°C"
                    subtext: backend.temp > 85 ? "⚠ HOT" : "OK"
                    buttonColor: backend.temp > 85 ? "#e74c3c" : "#0d89ef"
                }
            }

            // ── Status indicator (top-right corner) ──────────────────
            Text {
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.margins: 10
                text: backend.status
                color: backend.connected ? "#2ecc71" : "#f39c12"
                font.pixelSize: 12
                font.bold: true
            }
        }
    }

    // ── Reusable Components ────────────────────────────────────────────

    component CircularGauge: Item {
        property real  minValue:   0
        property real  maxValue:   100
        property real  value:      0
        property string unit:      ""
        property string label:     ""
        property color gaugeColor: '#0d89ef'

        Rectangle {
            anchors.fill: parent
            color: "#1a1a1a"
            radius: width / 2
            border.color: gaugeColor
            border.width: 6

            Column {
                anchors.centerIn: parent
                spacing: 5

                Text {
                    text: label
                    color: '#e8f4f5'; font.pixelSize: 14; font.bold: true
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Text {
                    text: Math.round(value)
                    color: "#ecf0f1"
                    font.pixelSize: parent.parent.width > 150 ? 48 : 32
                    font.bold: true
                    anchors.horizontalCenter: parent.horizontalCenter
                }
                Text {
                    text: unit
                    color: '#f1f5f5'; font.pixelSize: 16
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            Rectangle {
                anchors.fill: parent
                color: "transparent"
                radius: width / 2
                border.color: Qt.lighter(gaugeColor, 1.3)
                border.width: 3
                opacity: 0.3
            }
        }
    }

    component InfoButton: Rectangle {
        property string title:       ""
        property string value:       ""
        property string subtext:     ""
        property color  buttonColor: "#3498db"

        color: buttonColor
        radius: 8
        border.color: Qt.lighter(buttonColor, 1.2)
        border.width: 2

        Column {
            anchors.centerIn: parent
            spacing: 3

            Text {
                text: title
                color: "#ecf0f1"; font.pixelSize: 12; font.bold: true
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: value
                color: "white"; font.pixelSize: 18; font.bold: true
                anchors.horizontalCenter: parent.horizontalCenter
            }
            Text {
                text: subtext
                color: "#bdc3c7"; font.pixelSize: 10
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: console.log(title + " clicked")
        }
    }
}