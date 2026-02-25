import QtQuick 2.15
import QtQuick.Window 2.15
import QtQuick.Layouts 1.15
import QtQuick.Controls 2.15

Window {
    visible: true
    width: 800
    height: 480
    color: "#1e1e1e"
    title: "Tufts Electric Racing"
    
    // Background gradient
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#1a1a1a" }
            GradientStop { position: 1.0; color: "#000000" }
        }
    }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 20
        spacing: 15

        // --- HEADER ---
        RowLayout {
            Layout.fillWidth: true
            Text {
                text: "TUFTS ELECTRIC RACING"
                color: "#e74c3c" // Racing red
                font.pixelSize: 24
                font.bold: true
            }
            Item { Layout.fillWidth: true } // Spacer
            Text {
                text: backend.status
                color: backend.connected ? "#2ecc71" : "#f1c40f"
                font.pixelSize: 18
                font.bold: true
            }
        }

        // --- MAIN GAUGE (Speed) ---
        Item {
            Layout.fillWidth: true
            Layout.fillHeight: true
            
            Rectangle {
                width: 260; height: 260
                radius: 130
                color: "#111"
                border.color: backend.speed > 50 ? "#e74c3c" : "#3498db"
                border.width: 8
                anchors.centerIn: parent

                Column {
                    anchors.centerIn: parent
                    Text {
                        text: Math.round(backend.speed)
                        color: "white"
                        font.pixelSize: 100
                        font.bold: true
                        anchors.horizontalCenter: parent
                    }
                    Text {
                        text: "MPH"
                        color: "#aaa"
                        font.pixelSize: 24
                        font.bold: true
                        anchors.horizontalCenter: parent
                    }
                }
            }
        }

        // --- LOWER STATS ROW ---
        RowLayout {
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            spacing: 20
            
            // Reusable Info Box
            component InfoBox: Rectangle {
                property string label: ""
                property string value: ""
                property string unit: ""
                property color valueColor: "white"
                
                Layout.fillWidth: true; Layout.fillHeight: true
                color: "#2c2c2c"; radius: 10
                
                Column {
                    anchors.centerIn: parent
                    Text { text: parent.label; color: "#888"; font.pixelSize: 14; anchors.horizontalCenter: parent }
                    Row {
                        anchors.horizontalCenter: parent
                        spacing: 5
                        Text { text: parent.value; color: parent.valueColor; font.pixelSize: 32; font.bold: true }
                        Text { text: parent.unit; color: "#aaa"; font.pixelSize: 18; anchors.bottom: parent.bottom; anchors.bottomMargin: 4 }
                    }
                }
            }

            InfoBox { label: "POWER"; value: backend.power; unit: "kW" }
            InfoBox { 
                label: "VOLTAGE"; value: backend.voltage; unit: "V"; 
                valueColor: backend.voltage < 88 ? "#f39c12" : "white" 
            }
            InfoBox { 
                label: "MOTOR TEMP"; value: backend.temp; unit: "°C"; 
                valueColor: backend.temp > 70 ? "#e74c3c" : "#2ecc71" 
            }
        }
    }
}