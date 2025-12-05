#ifndef CAN_DATA_READER_QT_BRIDGE_H
#define CAN_DATA_READER_QT_BRIDGE_H

#include <QObject>
#include <QTimer>
#include <QDebug>
#include <cmath> // For std::isnan

// IMPORTANT: This now includes the proper declarations from the core C++ file.
#include "can_data_reader.h" // Includes SensorID enum, initialize_database, get_sensor_value, etc.

// We no longer need the manual 'extern' declarations or the redundant 'SensorID_Bridge' enum.

// --- QWidget Data Bridge Class ---
// This class handles the real-time polling and acts as the data source for the GUI.
class CanBridge : public QObject {
    Q_OBJECT

public:
    explicit CanBridge(QObject *parent = nullptr) : QObject(parent) {
        // 1. Initialize the CAN reader (runs the simulation setup and DB init)
        // This function is now correctly linked from can_data_reader.cpp
        initialize_database();
        qDebug() << "CAN Data Bridge initialized and database setup complete.";

        // 2. Setup Timer for GUI Refresh (200ms)
        m_timer = new QTimer(this);
        connect(m_timer, &QTimer::timeout, this, &CanBridge::pollAndUpdate);
        m_timer->start(200); // 5 updates per second (200ms)

        // Initial value is NaN, which the GUI displays as "---"
        m_engineTemp = std::nanf(""); 
    }

    // --- Q_PROPERTY Definition ---
    Q_PROPERTY(float engineTemp READ getEngineTemp NOTIFY engineTempChanged)

    float getEngineTemp() const {
        return m_engineTemp;
    }

public slots:
    // Slot connected to the QTimer to poll the latest data and emit signal
    void pollAndUpdate() {
        // *** REAL-TIME DUMMY IMPLEMENTATION ***
        // We use a simple counter to simulate data changes for a real-time effect 
        // on macOS where the real CAN hardware isn't available.
        
        static int cycle = 0;
        // Simulates a slight temperature change between 84.5 and 85.5 degrees C.
        float newTemp = 85.0f + 0.5f * sin((float)cycle / 10.0f); 
        
        // Use the ID defined in can_data_reader.h
        // In the simulation, we are bypassing the core reader's storage for simplicity
        // and generating the value here. If we wanted to use the core reader's storage,
        // we'd have to ensure it was updated asynchronously, which is complex for a
        // single-threaded example.
        
        if (std::isnan(m_engineTemp) || newTemp != m_engineTemp) {
            m_engineTemp = newTemp;
            emit engineTempChanged(m_engineTemp);
        }
        cycle++;
    }

signals:
    void engineTempChanged(float newTemp);

private:
    float m_engineTemp = std::nanf("");
    QTimer *m_timer;
};

#endif // CAN_DATA_READER_QT_BRIDGE_H