#!/usr/bin/env python3
# qml_display.py
# Purpose: QML-based display backend for electric racing car
# usage: python3 display/qml_display.py

import sys
import os
from PyQt5.QtCore import QObject, pyqtSignal, pyqtProperty, QTimer, QUrl
from PyQt5.QtWidgets import QApplication
from PyQt5.QtQml import QQmlApplicationEngine

# Add core paths to find can_getter
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'core'))
sys.path.insert(0, os.path.join(os.path.dirname(__file__), '..', 'config'))

# Try to import CANGetter
try:
    from can_getter import CANGetter
    CAN_AVAILABLE = True
except ImportError:
    print("Warning: can_getter module not found. Running in UI-only mode.")
    CAN_AVAILABLE = False

class DashboardBackend(QObject):
    """
    Bridges Python CAN data to QML.
    QML will bind to these properties and update automatically.
    """
    # Define Signals (notify QML when data changes)
    speedChanged = pyqtSignal(float)
    tempChanged = pyqtSignal(float)
    voltageChanged = pyqtSignal(float)
    powerChanged = pyqtSignal(float)
    statusChanged = pyqtSignal(str)
    connectionStatusChanged = pyqtSignal(bool)

    def __init__(self):
        super().__init__()
        # Initial values
        self._speed = 0.0
        self._temp = 0.0
        self._voltage = 0.0
        self._power = 0.0
        self._status = "Initializing..."
        self._connected = False
        
        # Connect to Shared Memory
        self.connect_shared_memory()

        # Timer to read data (30fps = 33ms)
        self.timer = QTimer()
        self.timer.timeout.connect(self.update_data)
        self.timer.start(33)

    def connect_shared_memory(self):
        if not CAN_AVAILABLE:
            self._status = "Missing libraries"
            self.statusChanged.emit(self._status)
            return

        try:
            self.can = CANGetter()
            self._connected = True
            self._status = "System Online"
            self.connectionStatusChanged.emit(True)
            self.statusChanged.emit(self._status)
            print("✓ Connected to shared memory")
        except FileNotFoundError:
            self._connected = False
            self._status = "Waiting for CAN..."
            self.connectionStatusChanged.emit(False)
            self.statusChanged.emit(self._status)
        except Exception as e:
            print(f"Error connecting: {e}")
            self._connected = False

    def update_data(self):
        # Try to reconnect if lost
        if not self._connected:
            if CAN_AVAILABLE:
                try:
                    self.can = CANGetter()
                    self._connected = True
                    self.connectionStatusChanged.emit(True)
                    self._status = "System Online"
                    self.statusChanged.emit(self._status)
                except:
                    return
            else:
                # Simulate data for UI testing if libraries missing
                self._speed = (self._speed + 0.5) % 100
                self.speedChanged.emit(self._speed)
                return

        try:
            # Read from Shared Memory using your existing getters
            new_speed = self.can.get_speed_mph()
            new_temp = self.can.get_motor_temp()
            new_volt = self.can.get_dc_voltage()
            new_power = self.can.get_power() / 1000.0  # Convert W to kW
            # ---- UPDATE + EMIT ----
            self._speed = new_speed
            self.speedChanged.emit(self._speed)

            self._temp = new_temp
            self.tempChanged.emit(self._temp)

            self._voltage = new_volt
            self.voltageChanged.emit(self._voltage)

            self._power = new_power
            self.powerChanged.emit(self._power)

        except Exception as e:
            # If reading fails, assume disconnected
            self._connected = False
            self.connectionStatusChanged.emit(False)

    # --- Properties accessible from QML ---
    
    @pyqtProperty(float, notify=speedChanged)
    def speed(self): return self._speed

    @pyqtProperty(float, notify=tempChanged)
    def temp(self): return self._temp

    @pyqtProperty(float, notify=voltageChanged)
    def voltage(self): return self._voltage

    @pyqtProperty(float, notify=powerChanged)
    def power(self): return self._power
        
    @pyqtProperty(str, notify=statusChanged)
    def status(self): return self._status

    @pyqtProperty(bool, notify=connectionStatusChanged)
    def connected(self): return self._connected

def main():
    app = QApplication(sys.argv)
    engine = QQmlApplicationEngine()

    backend = DashboardBackend()
    engine.rootContext().setContextProperty("backend", backend)

    # Load QML file
    qml_file = os.path.join(os.path.dirname(__file__), 'dashboard.qml')
    engine.load(QUrl.fromLocalFile(qml_file))

    if not engine.rootObjects():
        sys.exit(-1)

    sys.exit(app.exec_())

if __name__ == "__main__":
    main()