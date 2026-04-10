#!/usr/bin/env python3
# qml_display.py
# Purpose: QML-based display backend for electric racing car
# usage: python3 display/qml_display.py

import sys
import os
import time
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
    rpmChanged = pyqtSignal(float)
    tempChanged = pyqtSignal(float)
    voltageChanged = pyqtSignal(float)
    powerChanged = pyqtSignal(float)
    batteryPercentChanged = pyqtSignal(float)
    mileageChanged = pyqtSignal(float)
    motorStateChanged = pyqtSignal(str)
    directionChanged = pyqtSignal(str)
    statusChanged = pyqtSignal(str)
    connectionStatusChanged = pyqtSignal(bool)
    faultActiveChanged = pyqtSignal(bool)
    faultCodeChanged = pyqtSignal(str)

    def __init__(self):
        super().__init__()
        # Initial values
        self._speed = 0.0
        self._rpm = 0.0
        self._temp = 0.0
        self._voltage = 0.0
        self._power = 0.0
        self._battery_percent = 0.0
        self._mileage = 0.0
        self._motor_state = "IDLE"
        self._direction = "FWD"
        self._status = "Initializing..."
        self._connected = False
        
        # Mileage tracking
        self._last_update_time = time.time()
        
        # Battery voltage range (adjust for your system)
        self.VOLTAGE_MIN = 300.0  # 0% battery
        self.VOLTAGE_MAX = 400.0  # 100% battery
        # falt tracking
        self._fault_active = False
        self._fault_code = ""
        
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
                import math
                self._speed = abs(30 * math.sin(time.time() / 2))
                self._rpm = abs(3000 * math.sin(time.time() / 2))
                self._temp = 50.0 + 20.0 * math.sin(time.time() / 3)
                self._voltage = 90.0 + 5.0 * math.cos(time.time() / 2)
                self._battery_percent = 50.0 + 30.0 * math.sin(time.time() / 5)
                self.speedChanged.emit(self._speed)
                self.rpmChanged.emit(self._rpm)
                self.tempChanged.emit(self._temp)
                self.voltageChanged.emit(self._voltage)
                self.batteryPercentChanged.emit(self._battery_percent)
                return

        try:
            current_time = time.time()
            dt = current_time - self._last_update_time
            self._last_update_time = current_time

            # Read from Shared Memory
            new_speed = self.can.get_speed_mph()
            new_rpm = self.can.get_motor_speed()
            new_temp = self.can.get_bms_max_temp()
            new_volt = self.can.get_pack_voltage()
            new_power = self.can.get_power() / 1000.0  # Convert W to kW
            
            # Calculate battery percentage from voltage
            new_battery_percent = ((new_volt - self.VOLTAGE_MIN) / 
                                  (self.VOLTAGE_MAX - self.VOLTAGE_MIN)) * 100.0
            new_battery_percent = max(0.0, min(100.0, new_battery_percent))
            
            # Update mileage (integrate speed over time)
            # Speed is in MPH, dt is in seconds
            # miles = mph * (hours) = mph * (seconds / 3600)
            self._mileage += new_speed * (dt / 3600.0)
            
            # Get motor state
            vsm_state = self.can.get_vsm_state()
            state_names = {0: "IDLE", 1: "INIT", 2: "READY", 3: "RUNNING", 
                          4: "ERROR", 5: "ACTIVE"}
            new_motor_state = state_names.get(int(vsm_state), "UNKNOWN")
            
            # Get direction
            new_direction = self.can.get_direction_text()

            # Update and emit all changes
            if self._speed != new_speed:
                self._speed = new_speed
                self.speedChanged.emit(self._speed)
            
            if self._rpm != new_rpm:
                self._rpm = new_rpm
                self.rpmChanged.emit(self._rpm)

            if self._temp != new_temp:
                self._temp = new_temp
                self.tempChanged.emit(self._temp)

            if self._voltage != new_volt:
                self._voltage = new_volt
                self.voltageChanged.emit(self._voltage)

            if self._power != new_power:
                self._power = new_power
                self.powerChanged.emit(self._power)
            
            if self._battery_percent != new_battery_percent:
                self._battery_percent = new_battery_percent
                self.batteryPercentChanged.emit(self._battery_percent)
            
            self.mileageChanged.emit(self._mileage)
            
            if self._motor_state != new_motor_state:
                self._motor_state = new_motor_state
                self.motorStateChanged.emit(self._motor_state)
            
            if self._direction != new_direction:
                self._direction = new_direction
                self.directionChanged.emit(self._direction)
            # Fault tracking
            run_lo, run_hi = self.can.get_fault_codes()
            fault_active = run_lo != 0 or run_hi != 0
            fault_str = f"RUN: 0x{run_lo:04X} / 0x{run_hi:04X}" if fault_active else ""

            if self._fault_active != fault_active:
                self._fault_active = fault_active
                self.faultActiveChanged.emit(self._fault_active)
            if self._fault_code != fault_str:
                self._fault_code = fault_str
                self.faultCodeChanged.emit(self._fault_code)

        except Exception as e:
            print(f"Error reading data: {e}")
            self._connected = False
            self.connectionStatusChanged.emit(False)

    # --- Properties accessible from QML ---
    
    @pyqtProperty(float, notify=speedChanged)
    def speed(self): return self._speed
    
    @pyqtProperty(float, notify=rpmChanged)
    def rpm(self): return self._rpm

    @pyqtProperty(float, notify=tempChanged)
    def temp(self): return self._temp

    @pyqtProperty(float, notify=voltageChanged)
    def voltage(self): return self._voltage

    @pyqtProperty(float, notify=powerChanged)
    def power(self): return self._power
    
    @pyqtProperty(float, notify=batteryPercentChanged)
    def batteryPercent(self): return self._battery_percent
    
    @pyqtProperty(float, notify=mileageChanged)
    def mileage(self): return self._mileage
    
    @pyqtProperty(str, notify=motorStateChanged)
    def motorState(self): return self._motor_state
    
    @pyqtProperty(str, notify=directionChanged)
    def direction(self): return self._direction
        
    @pyqtProperty(str, notify=statusChanged)
    def status(self): return self._status

    @pyqtProperty(bool, notify=connectionStatusChanged)
    def connected(self): return self._connected
    # Fault properties
    @pyqtProperty(bool, notify=faultActiveChanged)
    def faultActive(self): return self._fault_active

    @pyqtProperty(str, notify=faultCodeChanged)
    def faultCode(self): return self._fault_code

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