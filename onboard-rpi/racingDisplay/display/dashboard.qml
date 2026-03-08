import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

Window {
    visible: true
    width: 800
    height: 480
    color: "#0a0a0a"
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
            anchors.margins: 10

            // Top Battery Gauge
            CircularGauge {
                id: batteryGauge
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 20
                width: 140
                height: 140
                
                minValue: 0
                maxValue: 100
                value: backend.batteryPercent
                unit: "%"
                label: "VOLTAGE"
                gaugeColor: backend.batteryPercent > 20 ? "#2ecc71" : "#e74c3c"
            }

            // Left MPH Gauge
            CircularGauge {
                id: speedGauge
                anchors.left: parent.left
                anchors.leftMargin: 0
                anchors.verticalCenter: parent.verticalCenter
                width: 180
                height: 180
                
                minValue: 0
                maxValue: 60
                value: backend.speed
                unit: "MPH"
                label: "SPEED"
                gaugeColor: backend.speed > 50 ? "#e74c3c" : "#3498db"
            }

            // Right RPM Gauge
            CircularGauge {
                id: rpmGauge
                anchors.right: parent.right
                anchors.rightMargin: 0
                anchors.verticalCenter: parent.verticalCenter
                width: 180
                height: 180
                
                minValue: 0
                maxValue: 6000
                value: backend.rpm
                unit: "RPM"
                label: "MOTOR"
                gaugeColor: backend.rpm > 5000 ? "#e74c3c" : "#f39c12"
            }

            // Center Mileage Display
            Rectangle {
                id: mileageBox
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter
                anchors.verticalCenterOffset: 200
                width: 180
                height: 80
                color: "#2c3e50"
                radius: 10
                border.color: "#34495e"
                border.width: 2

                Column {
                    anchors.centerIn: parent
                    spacing: 5
                    
                    Text {
                        text: "MILEAGE"
                        color: "#95a5a6"
                        font.pixelSize: 14
                        font.bold: true
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                    
                    Text {
                        text: backend.mileage.toFixed(2) + " mi"
                        color: "#ecf0f1"
                        font.pixelSize: 28
                        font.bold: true
                        anchors.horizontalCenter: parent.horizontalCenter
                    }
                }
            }

            // Bottom Info Buttons
            Row {
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 20
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: 20

                // Motor Info Button
                InfoButton {
                    width: 180
                    height: 80
                    title: "MOTOR"
                    value: backend.motorState
                    subtext: backend.direction
                    buttonColor: "#27ae60"
                }

                // Temperature Button
                InfoButton {
                    width: 180
                    height: 80
                    title: "TEMP"
                    value: backend.temp.toFixed(1) + "°C"
                    subtext: backend.temp > 85 ? "HOT" : "OK"
                    buttonColor: backend.temp > 85 ? "#e74c3c" : "#16a085"
                }
            }

            // Status Text (top right corner)
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

    // Reusable Circular Gauge Component
    component CircularGauge: Item {
        property real minValue: 0
        property real maxValue: 100
        property real value: 0
        property string unit: ""
        property string label: ""
        property color gaugeColor: "#3498db"

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
                    color: "#7f8c8d"
                    font.pixelSize: 14
                    font.bold: true
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
                    color: "#95a5a6"
                    font.pixelSize: 16
                    anchors.horizontalCenter: parent.horizontalCenter
                }
            }

            // Progress arc (simplified - full implementation would use Canvas)
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

    // Reusable Info Button Component
    component InfoButton: Rectangle {
        property string title: ""
        property string value: ""
        property string subtext: ""
        property color buttonColor: "#3498db"

        color: buttonColor
        radius: 8
        border.color: Qt.lighter(buttonColor, 1.2)
        border.width: 2

        Column {
            anchors.centerIn: parent
            spacing: 3

            Text {
                text: title
                color: "#ecf0f1"
                font.pixelSize: 12
                font.bold: true
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: value
                color: "white"
                font.pixelSize: 18
                font.bold: true
                anchors.horizontalCenter: parent.horizontalCenter
            }

            Text {
                text: subtext
                color: "#bdc3c7"
                font.pixelSize: 10
                anchors.horizontalCenter: parent.horizontalCenter
            }
        }

        MouseArea {
            anchors.fill: parent
            onClicked: {
                console.log(title + " clicked")
            }
        }
    }
}